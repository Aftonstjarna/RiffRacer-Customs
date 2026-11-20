#pragma once
// In-app updater. Stable releases are the default channel; beta is a separate
// opt-in channel built from GitHub prereleases only. A package is downloaded
// to a temporary file, checked against the release SHA-256, and only then
// installed. Failures leave the running editor untouched.
#include "update_channel.hpp"
#include <gtk/gtk.h>
#include <curl/curl.h>
#include <atomic>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace livery {
namespace update {

inline std::string settingsPath() {
    return (fs::u8path(g_get_user_config_dir()) / "reriff-livery-maker" / "settings.ini").u8string();
}

struct Settings {
    Channel channel = Channel::Stable;
    bool checkAtStartup = true;
    long long lastCheck = 0;
};

inline Settings readSettings() {
    Settings result;
    result.channel = installedBeta(CUSTOMS_VERSION) ? Channel::Beta : Channel::Stable;
    auto *file = g_key_file_new();
    if (g_key_file_load_from_file(file, settingsPath().c_str(), G_KEY_FILE_NONE, nullptr)) {
        auto *channel = g_key_file_get_string(file, "Update", "Channel", nullptr);
        if (channel) {
            result.channel = channelFromKey(channel);
            g_free(channel);
        }
        GError *error = nullptr;
        auto startup = g_key_file_get_boolean(file, "Update", "CheckAtStartup", &error);
        if (error) g_error_free(error);
        else result.checkAtStartup = startup != 0;
        error = nullptr;
        auto last = g_key_file_get_int64(file, "Update", "LastCheck", &error);
        if (error) g_error_free(error);
        else result.lastCheck = last;
    }
    g_key_file_unref(file);
    return result;
}

inline void writeSettings(const Settings &settings) {
    auto *file = g_key_file_new();
    g_key_file_load_from_file(file, settingsPath().c_str(), G_KEY_FILE_KEEP_COMMENTS, nullptr);
    g_key_file_set_string(file, "Update", "Channel", channelKey(settings.channel));
    g_key_file_set_boolean(file, "Update", "CheckAtStartup", settings.checkAtStartup);
    g_key_file_set_int64(file, "Update", "LastCheck", settings.lastCheck);
    gsize length = 0;
    auto *data = g_key_file_to_data(file, &length, nullptr);
    auto path = settingsPath();
    bool ok = g_mkdir_with_parents(fs::u8path(path).parent_path().u8string().c_str(), 0700) == 0 &&
              g_file_set_contents(path.c_str(), data, gssize(length), nullptr);
    g_free(data);
    g_key_file_unref(file);
    require(ok, "Cannot save update settings");
}

inline void ensureCurl() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

struct Transfer {
    std::string *text = nullptr;
    FILE *file = nullptr;
    unsigned long long received = 0;
    unsigned long long maximum = maximumMetadataBytes;
    std::atomic<bool> *cancel = nullptr;
    std::function<void(unsigned long long, unsigned long long)> progress;
    const char *accept = nullptr;
    long status = 0;
    char error[CURL_ERROR_SIZE] = {0};
};

inline size_t transferWrite(char *data, size_t size, size_t count, void *user) {
    auto &transfer = *static_cast<Transfer *>(user);
    size_t bytes = size * count;
    if (transfer.cancel && transfer.cancel->load()) return 0;
    if (bytes > transfer.maximum - transfer.received) {
        std::snprintf(transfer.error, sizeof(transfer.error), "The download exceeds the allowed size");
        return 0;
    }
    if (transfer.file) {
        if (std::fwrite(data, 1, bytes, transfer.file) != bytes) {
            std::snprintf(transfer.error, sizeof(transfer.error), "Cannot save the download");
            return 0;
        }
    } else {
        transfer.text->append(data, bytes);
    }
    transfer.received += bytes;
    return bytes;
}

inline int transferProgress(void *user, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    auto &transfer = *static_cast<Transfer *>(user);
    if (transfer.cancel && transfer.cancel->load()) return 1;
    if (transfer.progress) transfer.progress(now > 0 ? (unsigned long long)now : 0, total > 0 ? (unsigned long long)total : 0);
    return 0;
}

inline void request(const std::string &url, Transfer &transfer) {
    ensureCurl();
    CURL *curl = curl_easy_init();
    require(curl != nullptr, "Cannot initialize the network client");
    auto agent = std::string("RiffRacer-Customs/") + CUSTOMS_VERSION;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)transfer.maximum);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, transferWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transferProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, transfer.error);
    curl_slist *headers = nullptr;
    if (transfer.accept) headers = curl_slist_append(headers, (std::string("Accept: ") + transfer.accept).c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode result = curl_easy_perform(curl);
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &transfer.status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (transfer.cancel && transfer.cancel->load()) throw std::runtime_error("The update was canceled");
    require(result == CURLE_OK, transfer.error[0] ? transfer.error : curl_easy_strerror(result));
    require(transfer.status >= 200 && transfer.status < 300,
            "GitHub returned HTTP " + std::to_string(transfer.status));
}

inline std::string fetchText(const std::string &url, unsigned long long maximum,
                             std::atomic<bool> *cancel, const char *accept = nullptr) {
    std::string body;
    Transfer transfer;
    transfer.text = &body;
    transfer.maximum = maximum;
    transfer.cancel = cancel;
    transfer.accept = accept;
    request(url, transfer);
    return body;
}

inline FILE *openFile(const fs::path &path) {
#ifdef _WIN32
    return _wfopen(path.wstring().c_str(), L"wb");
#else
    return std::fopen(path.c_str(), "wb");
#endif
}

inline void downloadTo(const std::string &url, const fs::path &path, unsigned long long maximum,
                       std::atomic<bool> *cancel, const std::function<void(unsigned long long, unsigned long long)> &progress) {
    FILE *file = openFile(path);
    require(file != nullptr, "Cannot create the download file");
    Transfer transfer;
    transfer.file = file;
    transfer.maximum = maximum;
    transfer.cancel = cancel;
    transfer.progress = progress;
    try {
        request(url, transfer);
    } catch (...) {
        std::fclose(file);
        std::error_code ignored;
        fs::remove(path, ignored);
        throw;
    }
    require(std::fclose(file) == 0, "Cannot finish writing the download");
}

inline std::string fileSha256(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot read the downloaded package");
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    require(context != nullptr, "Cannot initialize SHA-256");
    std::array<unsigned char, 32> digest{};
    unsigned length = 0;
    bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;
    std::array<char, 65536> block{};
    while (ok && input) {
        input.read(block.data(), block.size());
        auto count = input.gcount();
        if (count > 0) ok = EVP_DigestUpdate(context, block.data(), size_t(count)) == 1;
    }
    ok = ok && input.eof() && EVP_DigestFinal_ex(context, digest.data(), &length) == 1 && length == 32;
    EVP_MD_CTX_free(context);
    require(ok, "Cannot hash the downloaded package");
    std::ostringstream out;
    for (auto byte : digest) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return out.str();
}

inline void relaunch(const fs::path &program) {
    auto value = program.u8string();
    gchar *argv[] = {const_cast<gchar *>(value.c_str()), nullptr};
    GError *error = nullptr;
    bool ok = g_spawn_async(nullptr, argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &error);
    std::string message = error ? error->message : "Cannot restart the editor";
    if (error) g_error_free(error);
    require(ok, message);
}

#if defined(_WIN32)
inline std::string windowsQuote(const std::string &value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out + "'";
}

inline fs::path editorPath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
        require(length != 0, "Cannot locate the editor executable");
        if (length < buffer.size() - 1) break;
        buffer.resize(buffer.size() * 2);
    }
    return fs::path(buffer.data());
}
#endif

inline void installPackage(const fs::path &package) {
#if defined(_WIN32)
    auto executable = editorPath();
    auto directory = executable.parent_path();
    auto script = fs::temp_directory_path() /
                  ("RiffRacer-Customs-update-" + std::to_string(GetCurrentProcessId()) + ".ps1");
    auto result = (fs::u8path(g_get_user_config_dir()) / "reriff-livery-maker" / "update-result.txt").u8string();
    std::ofstream helper(script, std::ios::binary);
    require(bool(helper), "Cannot create the update helper");
    helper << "\xEF\xBB\xBF";
    helper << "$ErrorActionPreference='Stop'\n"
           << "$pidToWait=" << GetCurrentProcessId() << "\n"
           << "$package=" << windowsQuote(package.u8string()) << "\n"
           << "$destination=" << windowsQuote(directory.u8string()) << "\n"
           << "$executable=" << windowsQuote(executable.u8string()) << "\n"
           << "$version=" << windowsQuote(CUSTOMS_VERSION) << "\n"
           << "$result=" << windowsQuote(result) << "\n"
           << "function Write-Result([string]$text){New-Item -ItemType Directory -Force (Split-Path -Parent $result)|Out-Null;Set-Content -LiteralPath $result -Value $text -Encoding UTF8}\n"
           << "$stage=Join-Path $env:TEMP ('customs-update-'+[guid]::NewGuid().ToString('N'))\n"
           << "$backup=$destination+'.old-'+$pidToWait\n"
           << "try{\n"
           << "  try{Wait-Process -Id $pidToWait -Timeout 120 -ErrorAction Stop}catch{}\n"
           << "  Expand-Archive -LiteralPath $package -DestinationPath $stage -Force\n"
           << "  $folders=@(Get-ChildItem -LiteralPath $stage -Directory)\n"
           << "  if($folders.Count -ne 1){throw 'The update package does not contain exactly one application folder.'}\n"
           << "  if(Test-Path -LiteralPath $backup){throw 'A previous update left '+$backup+'; remove it and try again.'}\n"
           << "  Move-Item -LiteralPath $destination -Destination $backup -ErrorAction Stop\n"
           << "  try{Move-Item -LiteralPath $folders[0].FullName -Destination $destination -ErrorAction Stop}\n"
           << "  catch{\n"
           << "    if(Test-Path -LiteralPath $destination){Remove-Item -LiteralPath $destination -Recurse -Force -ErrorAction SilentlyContinue}\n"
           << "    Move-Item -LiteralPath $backup -Destination $destination -ErrorAction SilentlyContinue\n"
           << "    throw\n"
           << "  }\n"
           << "  Write-Result ('installed '+$version)\n"
           << "  Start-Process -FilePath $executable -WorkingDirectory $destination\n"
           << "  Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue\n"
           << "}catch{\n"
           << "  Write-Result ('failed '+$_.Exception.Message)\n"
           << "  try{Start-Process -FilePath $executable -WorkingDirectory $destination}catch{}\n"
           << "}finally{\n"
           << "  if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue}\n"
           << "  if(Test-Path -LiteralPath $package){Remove-Item -LiteralPath $package -Force -ErrorAction SilentlyContinue}\n"
           << "}\n";
    helper.close();
    require(bool(helper), "Cannot write the update helper");
    auto scriptPath = script.u8string();
    gchar *argv[] = {const_cast<gchar *>("powershell.exe"), const_cast<gchar *>("-NoProfile"),
                     const_cast<gchar *>("-NonInteractive"), const_cast<gchar *>("-ExecutionPolicy"),
                     const_cast<gchar *>("Bypass"), const_cast<gchar *>("-WindowStyle"),
                     const_cast<gchar *>("Hidden"), const_cast<gchar *>("-File"),
                     const_cast<gchar *>(scriptPath.c_str()), nullptr};
    auto workingDirectory = fs::temp_directory_path().u8string();
    GError *error = nullptr;
    bool ok = g_spawn_async(workingDirectory.c_str(), argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr,
                            nullptr, &error);
    std::string message = error ? error->message : "Cannot start the update helper";
    if (error) g_error_free(error);
    require(ok, message + ". Install the downloaded package manually.");
#else
    const char *running = g_getenv("APPIMAGE");
    require(running && *running,
            "This build is not an AppImage. Download the new package from the release page and replace this build manually.");
    std::error_code error;
    auto target = fs::canonical(fs::u8path(running), error);
    require(!error, "Cannot locate the running AppImage: " + error.message());
    auto incoming = target.parent_path() / ("." + target.filename().string() + ".new");
    fs::copy_file(package, incoming, fs::copy_options::overwrite_existing, error);
    require(!error, "Cannot write next to the AppImage (" + error.message() +
                        "). Move the AppImage to a writable folder or install the update manually.");
    fs::permissions(incoming,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                        fs::perms::others_read | fs::perms::others_exec,
                    error);
    if (error) {
        fs::remove(incoming);
        require(false, "Cannot set permissions on the update package: " + error.message());
    }
    fs::rename(incoming, target, error);
    if (error) {
        fs::remove(incoming);
        require(false, "Cannot replace the running AppImage (" + error.message() +
                           "). Install the update manually.");
    }
    relaunch(target);
#endif
}

enum class Stage { Idle, Checking, Ready, Downloading, Verifying, Installing, Installed, Failed };

struct State {
    std::mutex mutex;
    Stage stage = Stage::Idle;
    std::string message, error, version, notes, tag, asset, sha;
    unsigned long long size = 0;
    bool available = false, installable = false, explicitSize = false;
    bool manual = false, cacheCheck = false;
    int percent = -1;
    std::atomic<bool> cancel{false};
};

inline void setStage(const std::shared_ptr<State> &state, Stage stage, const std::string &message) {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->stage = stage;
    state->message = message;
    state->error.clear();
}

inline void setError(const std::shared_ptr<State> &state, const std::string &error) {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->stage = Stage::Failed;
    state->error = error;
    state->message.clear();
    state->percent = -1;
}

inline bool canceled(const std::shared_ptr<State> &state) { return state->cancel.load(); }

inline void checkWorker(std::shared_ptr<State> state, Channel channel, bool manual) {
    try {
        state->cancel = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->stage = Stage::Checking;
            state->manual = manual;
            state->available = false;
            state->installable = false;
            state->cacheCheck = false;
            state->version.clear();
            state->notes.clear();
            state->error.clear();
            state->percent = -1;
            state->message = channel == Channel::Beta ? "Checking for beta releases…" : "Checking for stable releases…";
        }
        std::optional<Candidate> best;
        Json chosen;
        bool found = false;
        auto consider = [&](const Json &page) {
            auto selected = selectFromPage(page, channel);
            if (!selected) return;
            if (found && !newer(*selected, *best)) return;
            best = *selected;
            chosen = releaseObject(page, selected->tag);
            found = true;
        };
        if (channel == Channel::Stable) {
            std::string body;
            try {
                body = fetchText(std::string(releasesApi) + "/latest", maximumMetadataBytes, &state->cancel,
                                 "application/vnd.github+json");
            } catch (const std::exception &error) {
                if (std::string(error.what()).find("HTTP 404") != std::string::npos)
                    throw std::runtime_error("No published stable release is available");
                throw;
            }
            consider(Json::parse(body));
        } else {
            for (unsigned page = 1; page <= 100; page++) {
                if (canceled(state)) throw std::runtime_error("The update check was canceled");
                auto body = fetchText(std::string(releasesApi) + "?per_page=100&page=" + std::to_string(page),
                                      maximumMetadataBytes, &state->cancel, "application/vnd.github+json");
                auto parsed = Json::parse(body);
                require(parsed.is_array(), "GitHub returned an invalid release list");
                consider(parsed);
                if (parsed.size() < 100) break;
                require(page < 100, "The GitHub release list exceeded the update check limit");
            }
        }
        require(found, channel == Channel::Beta ? "No published beta release is available yet"
                                                : "No published stable release is available");
        Release release;
        release.tag = best->tag;
        release.version = best->version;
        release.published = best->published;
        release.notes = best->notes;
        release.available = true;
        selectAsset(release, chosen);
        release.installable = needsInstall(release, CUSTOMS_VERSION, channel, installedBeta(CUSTOMS_VERSION));
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->stage = Stage::Ready;
            state->version = release.version;
            state->notes = release.notes;
            state->tag = release.tag;
            state->asset = release.asset;
            state->sha = release.sha256;
            state->size = release.size;
            state->explicitSize = release.explicitSize;
            state->available = true;
            state->installable = release.installable;
            state->cacheCheck = !release.installable;
            state->message = release.installable
                                 ? "Version " + release.version + " is available."
                                 : "RiffRacer Customs " + std::string(CUSTOMS_VERSION) +
                                       " is the latest published release on the " +
                                       (channel == Channel::Beta ? "beta" : "stable") + " channel.";
            state->percent = -1;
        }
    } catch (const std::exception &error) {
        if (canceled(state)) {
            setStage(state, Stage::Ready, "Update check canceled.");
            return;
        }
        setError(state, std::string("Update check failed: ") + error.what());
    }
}

inline void installWorker(std::shared_ptr<State> state) {
    fs::path package;
    try {
        Release release;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            release.tag = state->tag;
            release.version = state->version;
            release.asset = state->asset;
            release.sha256 = state->sha;
            release.size = state->size;
            release.explicitSize = state->explicitSize;
            release.notes = state->notes;
            require(state->available && state->installable, "There is no update to install");
            state->manual = true;
            state->stage = Stage::Downloading;
            state->error.clear();
            state->percent = 0;
            state->message = "Downloading " + release.asset + "…";
        }
        state->cancel = false;
        auto sums = fetchText(sumsUrl(release.tag), maximumChecksumBytes, &state->cancel);
        auto expected = checksumFor(sums, release.asset);
        if (!release.sha256.empty())
            require(lowerHex(release.sha256) == expected, "The package metadata and release checksum disagree");
        package = fs::temp_directory_path() / release.asset;
        downloadTo(releaseUrl(release.tag, release.asset), package, maximumPackageBytes, &state->cancel,
                   [&](unsigned long long now, unsigned long long total) {
                       std::lock_guard<std::mutex> lock(state->mutex);
                       state->percent = total > 0 ? int(now * 100 / total) : -1;
                   });
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->stage = Stage::Verifying;
            state->message = "Verifying the downloaded update…";
            state->percent = -1;
        }
        auto size = fs::file_size(package);
        if (release.explicitSize) require(size == release.size, "The downloaded package size does not match the release metadata");
        require(size <= maximumPackageBytes, "The downloaded package exceeds the update size limit");
        auto actual = fileSha256(package);
        if (lowerHex(actual) != expected) {
            std::error_code ignored;
            fs::remove(package, ignored);
            require(false, "The downloaded update did not match the release checksum");
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->stage = Stage::Installing;
            state->message = "Installing " + release.version + ". RiffRacer Customs will restart.";
            state->percent = -1;
        }
        installPackage(package);
        setStage(state, Stage::Installed, "Installed " + release.version + ". RiffRacer Customs is restarting.");
    } catch (const std::exception &error) {
        Stage stage;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            stage = state->stage;
        }
        std::string message = error.what();
        std::error_code ignored;
        if (!package.empty()) {
            if (stage == Stage::Installing && fs::exists(package, ignored))
                message += " The verified package is saved at " + package.u8string() + ".";
            else
                fs::remove(package, ignored);
        }
        if (canceled(state)) {
            setStage(state, Stage::Ready, "Update canceled; the installed editor is unchanged.");
            return;
        }
        setError(state, std::string("Update installation failed: ") + message);
    }
}

class UpdatePanel {
public:
    UpdatePanel(std::string version, std::function<bool()> mayInstall, std::function<void()> quit)
        : version_(std::move(version)), mayInstall_(std::move(mayInstall)), quit_(std::move(quit)),
          state_(std::make_shared<State>()) {
        settings_ = readSettings();
        ensureCurl();
        timer_ = g_timeout_add(200, +[](gpointer data) -> gboolean {
            static_cast<UpdatePanel *>(data)->refresh();
            return G_SOURCE_CONTINUE;
        }, this);
    }
    ~UpdatePanel() {
        state_->cancel = true;
        if (timer_) g_source_remove(timer_);
    }
    UpdatePanel(const UpdatePanel &) = delete;
    UpdatePanel &operator=(const UpdatePanel &) = delete;

    void buildButton(GtkWidget *row) {
        button_ = gtk_button_new_with_label("Check for updates…");
        gtk_box_pack_end(GTK_BOX(row), button_, false, false, 0);
        gtk_widget_set_tooltip_text(button_, ("RiffRacer Customs " + version_ +
            ". Stable updates are recommended; beta updates are opt-in prereleases. Every package is verified with SHA-256 before installation.").c_str());
        g_signal_connect(button_, "clicked", G_CALLBACK(+[](GtkButton *, gpointer data) {
            static_cast<UpdatePanel *>(data)->openDialog();
        }), this);
        showResultFile();
    }

    void renderPreview(const fs::path &png) {
        createDialog();
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->stage = Stage::Ready;
            state_->manual = true;
            state_->available = true;
            state_->installable = true;
            state_->version = "0.4.0-beta.1";
            state_->message = "Version 0.4.0-beta.1 is available.";
            state_->notes = "Example release notes shown before installation.\n\nIn-game summary\n\n- Example change.\n- Example change.";
        }
        refresh();
        gtk_widget_show_all(dialog_);
        while (gtk_events_pending()) gtk_main_iteration();
        auto width = gtk_widget_get_allocated_width(dialog_);
        auto height = gtk_widget_get_allocated_height(dialog_);
        require(width > 0 && height > 0, "Update dialog has no layout");
        auto *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        auto *cr = cairo_create(surface);
        gtk_widget_draw(dialog_, cr);
        cairo_destroy(cr);
        require(cairo_surface_write_to_png(surface, png.u8string().c_str()) == CAIRO_STATUS_SUCCESS,
                "Cannot write the update dialog preview");
        cairo_surface_destroy(surface);
        closeDialog();
    }

    void autoCheck() {
        if (!settings_.checkAtStartup) return;
        auto now = (long long)std::time(nullptr);
        if (settings_.lastCheck > 0 && now - settings_.lastCheck >= 0 &&
            now - settings_.lastCheck < checkIntervalSeconds)
            return;
        startCheck(false);
    }

private:
    std::string version_;
    std::function<bool()> mayInstall_;
    std::function<void()> quit_;
    std::shared_ptr<State> state_;
    Settings settings_;
    guint timer_ = 0;
    GtkWidget *button_ = nullptr, *dialog_ = nullptr;
    GtkWidget *dialogChannel_ = nullptr, *dialogAutoCheck_ = nullptr, *status_ = nullptr;
    GtkWidget *progress_ = nullptr, *notes_ = nullptr, *install_ = nullptr, *checkButton_ = nullptr;
    bool syncing_ = false, pendingQuit_ = false, reported_ = false, cached_ = false;
    std::string notesShown_, buttonLabel_;

    void save() {
        try {
            writeSettings(settings_);
        } catch (const std::exception &) {
            // Settings are best-effort; a read-only profile must not block editing.
        }
    }

    void syncChannel() {
        syncing_ = true;
        if (dialogChannel_) gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialogChannel_), channelKey(settings_.channel));
        syncing_ = false;
    }

    void startCheck(bool manual) {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (state_->stage == Stage::Checking || state_->stage == Stage::Downloading ||
                state_->stage == Stage::Verifying || state_->stage == Stage::Installing)
                return;
        }
        state_->cancel = false;
        auto state = state_;
        auto channel = settings_.channel;
        std::thread([state, channel, manual] { checkWorker(state, channel, manual); }).detach();
    }

    void startInstall() {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (!state_->available || !state_->installable) return;
            if (state_->stage == Stage::Checking || state_->stage == Stage::Downloading ||
                state_->stage == Stage::Verifying || state_->stage == Stage::Installing)
                return;
        }
        if (mayInstall_ && !mayInstall_()) return;
        state_->cancel = false;
        auto state = state_;
        std::thread([state] { installWorker(state); }).detach();
    }

    void createDialog() {
        dialog_ = gtk_dialog_new_with_buttons("RiffRacer Customs Update", button_ ? GTK_WINDOW(gtk_widget_get_toplevel(button_)) : nullptr,
                                              GTK_DIALOG_MODAL, "Close", GTK_RESPONSE_CLOSE, nullptr);
        auto *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
        gtk_container_set_border_width(GTK_CONTAINER(content), 12);
        gtk_box_set_spacing(GTK_BOX(content), 8);
        auto *heading = gtk_label_new(("Installed version: " + version_).c_str());
        gtk_label_set_xalign(GTK_LABEL(heading), 0);
        gtk_box_pack_start(GTK_BOX(content), heading, false, false, 0);
        auto *channelHeading = gtk_label_new("Update channel");
        gtk_label_set_xalign(GTK_LABEL(channelHeading), 0);
        gtk_box_pack_start(GTK_BOX(content), channelHeading, false, false, 0);
        dialogChannel_ = gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dialogChannel_), channelKey(Channel::Stable), channelLabel(Channel::Stable));
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dialogChannel_), channelKey(Channel::Beta), channelLabel(Channel::Beta));
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialogChannel_), channelKey(settings_.channel));
        gtk_box_pack_start(GTK_BOX(content), dialogChannel_, false, false, 0);
        gtk_widget_set_tooltip_text(dialogChannel_, "Stable follows GitHub's latest non-prerelease release. Beta follows prereleases and is opt-in.");
        g_signal_connect(dialogChannel_, "changed", G_CALLBACK(+[](GtkComboBox *combo, gpointer data) {
            auto &panel = *static_cast<UpdatePanel *>(data);
            if (panel.syncing_) return;
            auto *id = gtk_combo_box_get_active_id(combo);
            panel.settings_.channel = channelFromKey(id ? id : "stable");
            panel.settings_.lastCheck = 0;
            panel.save();
            panel.startCheck(true);
        }), this);
        dialogAutoCheck_ = gtk_check_button_new_with_label("Check for updates at startup");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialogAutoCheck_), settings_.checkAtStartup);
        gtk_box_pack_start(GTK_BOX(content), dialogAutoCheck_, false, false, 0);
        g_signal_connect(dialogAutoCheck_, "toggled", G_CALLBACK(+[](GtkToggleButton *toggle, gpointer data) {
            auto &panel = *static_cast<UpdatePanel *>(data);
            panel.settings_.checkAtStartup = gtk_toggle_button_get_active(toggle);
            panel.save();
        }), this);
        status_ = gtk_label_new("");
        gtk_label_set_line_wrap(GTK_LABEL(status_), true);
        gtk_label_set_xalign(GTK_LABEL(status_), 0);
        gtk_label_set_selectable(GTK_LABEL(status_), true);
        gtk_box_pack_start(GTK_BOX(content), status_, false, false, 0);
        progress_ = gtk_progress_bar_new();
        gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(progress_), 0.08);
        gtk_box_pack_start(GTK_BOX(content), progress_, false, false, 0);
        auto *scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 150);
        gtk_widget_set_size_request(scroll, 460, 180);
        notes_ = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(notes_), false);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(notes_), false);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(notes_), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(notes_), 6);
        gtk_text_view_set_right_margin(GTK_TEXT_VIEW(notes_), 6);
        gtk_container_add(GTK_CONTAINER(scroll), notes_);
        gtk_box_pack_start(GTK_BOX(content), scroll, true, true, 0);
        auto *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        checkButton_ = gtk_button_new_with_label("Check now");
        install_ = gtk_button_new_with_label("Install update");
        gtk_widget_set_sensitive(install_, false);
        gtk_box_pack_start(GTK_BOX(actions), checkButton_, false, false, 0);
        gtk_box_pack_end(GTK_BOX(actions), install_, false, false, 0);
        gtk_box_pack_start(GTK_BOX(content), actions, false, false, 0);
        g_signal_connect(checkButton_, "clicked", G_CALLBACK(+[](GtkButton *, gpointer data) {
            static_cast<UpdatePanel *>(data)->startCheck(true);
        }), this);
        g_signal_connect(install_, "clicked", G_CALLBACK(+[](GtkButton *, gpointer data) {
            static_cast<UpdatePanel *>(data)->startInstall();
        }), this);
        syncChannel();
    }

    void closeDialog() {
        if (!dialog_) return;
        gtk_widget_destroy(dialog_);
        dialog_ = nullptr;
        status_ = progress_ = notes_ = install_ = checkButton_ = dialogChannel_ = dialogAutoCheck_ = nullptr;
        notesShown_.clear();
    }

    void openDialog() {
        if (dialog_) {
            gtk_window_present(GTK_WINDOW(dialog_));
            return;
        }
        reported_ = true;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->manual = true;
        }
        createDialog();
        gtk_widget_show_all(dialog_);
        startCheck(true);
        gtk_dialog_run(GTK_DIALOG(dialog_));
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (state_->stage == Stage::Checking || state_->stage == Stage::Downloading ||
                state_->stage == Stage::Verifying)
                state_->cancel = true;
        }
        closeDialog();
        if (pendingQuit_) quit_();
    }

    void refresh() {
        Stage stage;
        std::string message, error, version, notes;
        bool available, installable, manual, cache;
        int percent;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            stage = state_->stage;
            message = state_->message;
            error = state_->error;
            version = state_->version;
            notes = state_->notes;
            available = state_->available;
            installable = state_->installable;
            manual = state_->manual;
            cache = state_->cacheCheck;
            percent = state_->percent;
        }
        if (cache && !cached_ && stage == Stage::Ready) {
            cached_ = true;
            settings_.lastCheck = (long long)std::time(nullptr);
            save();
        }
        std::string label = stage == Stage::Ready && available && installable
                                ? "Update available: " + version
                                : "Check for updates…";
        if (button_ && label != buttonLabel_) {
            buttonLabel_ = label;
            gtk_button_set_label(GTK_BUTTON(button_), label.c_str());
            gtk_widget_set_tooltip_text(button_, stage == Stage::Ready && available && installable
                ? ("RiffRacer Customs " + version +
                   " is available. Open this window to read the release notes and install it.").c_str()
                : ("RiffRacer Customs " + version_ +
                   ". Stable updates are recommended; beta updates are opt-in prereleases. Every package is verified with SHA-256 before installation.").c_str());
        }
        if (stage == Stage::Ready && available && installable) reported_ = true;
        if (stage == Stage::Failed && !reported_ && !manual) reported_ = true;
        if (!dialog_) return;
        std::string text = error.empty() ? message : error;
        if (stage == Stage::Failed && !manual) text = "Update check failed; editing is unaffected.";
        gtk_label_set_text(GTK_LABEL(status_), text.c_str());
        if (notes != notesShown_) {
            notesShown_ = notes;
            auto *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes_));
            gtk_text_buffer_set_text(buffer, notes.c_str(), -1);
        }
        if (available && installable &&
            std::string(gtk_button_get_label(GTK_BUTTON(install_))).find(version) == std::string::npos)
            gtk_button_set_label(GTK_BUTTON(install_), ("Install " + version).c_str());
        bool busy = stage == Stage::Checking || stage == Stage::Downloading || stage == Stage::Verifying ||
                    stage == Stage::Installing;
        gtk_widget_set_sensitive(checkButton_, !busy);
        gtk_widget_set_sensitive(dialogChannel_, !busy);
        gtk_widget_set_sensitive(install_, available && installable && !busy);
        if (stage == Stage::Checking || stage == Stage::Verifying || stage == Stage::Installing) {
            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_));
        } else if (stage == Stage::Downloading) {
            if (percent >= 0) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_), percent / 100.0);
            else gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_));
        } else {
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_), 0);
        }
        if (dialog_)
            gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE,
                                              stage != Stage::Installing);
        if (stage == Stage::Installed && !pendingQuit_) {
            pendingQuit_ = true;
            if (dialog_) gtk_dialog_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE);
            else quit_();
        }
    }

    void showResultFile() {
        auto path = fs::u8path(g_get_user_config_dir()) / "reriff-livery-maker" / "update-result.txt";
        std::error_code error;
        if (!fs::exists(path, error)) return;
        auto text = read(path.u8string());
        fs::remove(path, error);
        std::string result(text.begin(), text.end());
        if (result.rfind("\xEF\xBB\xBF", 0) == 0) result.erase(0, 3);
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
        if (button_ && !result.empty())
            gtk_widget_set_tooltip_text(button_, ("Last update: " + result).c_str());
    }
};

} // namespace update
} // namespace livery
