#pragma once
// Release-selection rules for the in-app updater. Mirrors ReRiff's launcher:
// stable is the default channel and only sees GitHub's non-prerelease latest
// release; beta is opt-in and only sees prereleases, ordered by publication
// time rather than by semantic version. Tags use the v-prefixed release
// version, and every package is bound to an explicit filename, size and
// SHA-256 before installation.
#include "core.hpp"
#include <cctype>
#include <optional>

namespace livery {
namespace update {

inline constexpr const char *repository = "Aftonstjarna/RiffRacer-Customs";
inline constexpr const char *releasesApi = "https://api.github.com/repos/Aftonstjarna/RiffRacer-Customs/releases";
inline constexpr const char *downloadBase = "https://github.com/Aftonstjarna/RiffRacer-Customs/releases/download";
inline constexpr unsigned long long maximumPackageBytes = 256ULL * 1024 * 1024;
inline constexpr unsigned long long maximumMetadataBytes = 2ULL * 1024 * 1024;
inline constexpr unsigned long long maximumChecksumBytes = 64ULL * 1024;
inline constexpr long long checkIntervalSeconds = 15 * 60;

#ifndef CUSTOMS_VERSION
#define CUSTOMS_VERSION "development"
#endif

enum class Channel { Stable, Beta };

inline Channel channelFromKey(const std::string &key) {
    return key == "beta" ? Channel::Beta : Channel::Stable;
}
inline const char *channelKey(Channel channel) {
    return channel == Channel::Beta ? "beta" : "stable";
}
inline const char *channelLabel(Channel channel) {
    return channel == Channel::Beta ? "Beta — opt-in prereleases" : "Stable — recommended";
}

struct Candidate {
    std::string tag, version, published, notes;
    bool prerelease = false, draft = false;
};

struct Release {
    std::string tag, version, asset, sha256, notes, published;
    unsigned long long size = 0;
    bool available = false, installable = false, explicitSize = false;
};

inline bool validVersion(const std::string &version) {
    if (version.empty() || version == "." || version == "..") return false;
    for (unsigned char c : version)
        if (!(std::isalnum(c) || c == '.' || c == '-' || c == '_')) return false;
    return true;
}

// A hyphen with a non-empty suffix marks a prerelease: beta.2, rc.1, or the
// legacy numeric style. Stable versions have no suffix.
inline bool prereleaseVersion(const std::string &version) {
    auto dash = version.find('-');
    return dash != std::string::npos && dash + 1 < version.size();
}

inline std::string versionFromTag(const std::string &tag) {
    require(tag.size() > 1 && tag[0] == 'v', "Release tag is not a version tag");
    auto version = tag.substr(1);
    require(validVersion(version), "Release tag has unsupported characters");
    return version;
}

inline std::string assetName(const std::string &version) {
    require(validVersion(version), "Release version has unsupported characters");
#ifdef _WIN32
    return "RiffRacer-Customs-" + version + "-windows-x64.zip";
#else
    return "RiffRacer-Customs-" + version + "-anylinux-x86_64.AppImage";
#endif
}

inline std::string sumsName() { return "SHA256SUMS"; }

inline bool timestamp(const std::string &value) {
    if (value.size() != 20) return false;
    for (size_t i = 0; i < value.size(); i++) {
        char separator = i == 4 || i == 7 ? '-' : i == 10 ? 'T' : i == 13 || i == 16 ? ':' : i == 19 ? 'Z' : 0;
        if (separator ? value[i] != separator : !std::isdigit(static_cast<unsigned char>(value[i]))) return false;
    }
    return true;
}

inline bool hexDigest(const std::string &value) {
    if (value.size() != 64) return false;
    for (char c : value)
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

inline std::string lowerHex(std::string value) {
    for (char &c : value) c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

inline bool newer(const Candidate &candidate, const Candidate &best) {
    return candidate.published > best.published;
}

// Parse one entry, refusing any release whose channel flags or publication
// time are missing or malformed instead of guessing.
inline Candidate candidate(const Json &release) {
    require(release.is_object(), "GitHub returned a release that is not an object");
    require(release.contains("tag_name") && release["tag_name"].is_string(), "Release has no tag");
    require(release.contains("prerelease") && release["prerelease"].is_boolean(), "Release has no prerelease flag");
    require(release.contains("draft") && release["draft"].is_boolean(), "Release has no draft flag");
    require(release.contains("published_at") && release["published_at"].is_string(), "Release has no publication time");
    Candidate result;
    result.tag = release["tag_name"].get<std::string>();
    result.published = release["published_at"].get<std::string>();
    result.notes = release.value("body", std::string());
    require(timestamp(result.published), "Release publication time is malformed");
    result.draft = release["draft"].get<bool>();
    result.prerelease = release["prerelease"].get<bool>();
    result.version = versionFromTag(result.tag);
    require(prereleaseVersion(result.version) == result.prerelease,
            "Release tag and prerelease flag disagree");
    return result;
}

// Beta pages are ordered lists; keep the most recently published prerelease.
inline std::optional<Candidate> selectFromPage(const Json &page, Channel channel) {
    if (channel == Channel::Stable) {
        auto only = candidate(page);
        require(!only.prerelease && !only.draft, "The stable channel returned a prerelease");
        return only;
    }
    require(page.is_array(), "GitHub returned an invalid release list");
    std::optional<Candidate> best;
    for (const auto &entry : page) {
        auto item = candidate(entry);
        if (!item.prerelease || item.draft) continue;
        if (!best || newer(item, *best)) best = item;
    }
    return best;
}

inline void selectAsset(Release &release, const Json &metadata) {
    require(metadata.is_object(), "GitHub release metadata is invalid");
    require(metadata.contains("assets") && metadata["assets"].is_array(), "Release has no asset list");
    auto expected = assetName(release.version);
    const Json *match = nullptr;
    for (const auto &asset : metadata["assets"]) {
        if (!asset.is_object() || !asset.contains("name") || !asset["name"].is_string()) continue;
        if (asset["name"].get<std::string>() != expected) continue;
        require(match == nullptr, "Release contains more than one matching package");
        match = &asset;
    }
    require(match != nullptr, "Release does not contain " + expected);
    release.asset = expected;
    if (match->contains("size")) {
        require((*match)["size"].is_number_unsigned(), "Package size is invalid");
        release.size = (*match)["size"].get<unsigned long long>();
        require(release.size > 0 && release.size <= maximumPackageBytes, "Package size is not supported");
        release.explicitSize = true;
    }
    if (match->contains("digest") && !(*match)["digest"].is_null()) {
        require((*match)["digest"].is_string(), "Package digest is invalid");
        auto digest = (*match)["digest"].get<std::string>();
        require(digest.rfind("sha256:", 0) == 0 && hexDigest(digest.substr(7)), "Package digest is not SHA-256");
        release.sha256 = lowerHex(digest.substr(7));
    }
}

// Locate the release object a candidate was parsed from, so its assets can be
// validated without trusting a second, independently ordered list.
inline const Json &releaseObject(const Json &page, const std::string &tag) {
    if (page.is_object()) {
        require(page.contains("tag_name") && page["tag_name"].is_string() &&
                    page["tag_name"].get<std::string>() == tag,
                "Release metadata does not match the selected tag");
        return page;
    }
    require(page.is_array(), "GitHub returned an invalid release list");
    for (const auto &entry : page)
        if (entry.is_object() && entry.contains("tag_name") && entry["tag_name"].is_string() &&
            entry["tag_name"].get<std::string>() == tag)
            return entry;
    require(false, "The selected release disappeared from the GitHub response");
    throw std::runtime_error("unreachable");
}

inline std::string encodeComponent(const std::string &value) {
    static const char hex[] = "0123456789ABCDEF";
    require(!value.empty() && value != "." && value != "..", "Invalid release tag");
    std::string out;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') out += char(c);
        else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

inline std::string releaseUrl(const std::string &tag, const std::string &asset) {
    require(validVersion(asset) || asset == sumsName(), "Invalid release asset name");
    return std::string(downloadBase) + "/" + encodeComponent(tag) + "/" + asset;
}

inline std::string sumsUrl(const std::string &tag) { return releaseUrl(tag, sumsName()); }

// Parse "SHA256SUMS" exactly. The selected filename must appear once with a
// complete digest; duplicates or unrelated-looking lines cannot rescue a miss.
inline std::string checksumFor(const std::string &sums, const std::string &filename) {
    require(validVersion(filename), "Invalid package filename");
    std::optional<std::string> found;
    size_t start = 0;
    while (start <= sums.size()) {
        auto end = sums.find('\n', start);
        auto line = sums.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? sums.size() + 1 : end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 67) continue;
        auto digest = line.substr(0, 64);
        if (!hexDigest(digest)) continue;
        size_t name = 64;
        while (name < line.size() && (line[name] == ' ' || line[name] == '\t')) name++;
        if (name < line.size() && line[name] == '*') name++;
        if (line.substr(name) != filename) continue;
        require(!found || lowerHex(*found) == lowerHex(digest), "Release checksums disagree for " + filename);
        require(!found, "Release checksums list " + filename + " more than once");
        found = digest;
    }
    require(found.has_value(), "The release checksums do not list " + filename);
    return lowerHex(*found);
}

inline bool needsInstall(const Release &release, const std::string &installedVersion,
                         Channel requested, bool installedIsBeta) {
    if ((requested == Channel::Beta) != installedIsBeta) return true;
    return release.version != installedVersion;
}

inline bool installedBeta(const std::string &version) { return prereleaseVersion(version); }

} // namespace update
} // namespace livery
