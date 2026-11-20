// Portable tests for the updater's release-selection and verification rules.
#include "update_channel.hpp"
#include <iostream>

using namespace livery;
using namespace livery::update;

#define CHECK(condition) do {if (!(condition)) {std::cerr << "FAIL " << __LINE__ << ": " #condition "\n"; return 1;}} while (0)

template <class F> static bool refuses(F action) {
    try { action(); } catch (const std::exception &) { return true; }
    return false;
}

static std::string releaseJson(const std::string &tag, bool prerelease, bool draft, const std::string &published,
                               const std::string &body = "notes") {
    return "{\"tag_name\":\"" + tag + "\",\"prerelease\":" + (prerelease ? "true" : "false") +
           ",\"draft\":" + (draft ? "true" : "false") + ",\"published_at\":\"" + published +
           "\",\"body\":\"" + body + "\"}";
}

int main() {
    CHECK(prereleaseVersion("0.3.0-beta.2"));
    CHECK(prereleaseVersion("1.4.1-6"));
    CHECK(prereleaseVersion("0.4.0-rc.1"));
    CHECK(!prereleaseVersion("0.3.0"));
    CHECK(!prereleaseVersion("0.3.0-"));
    CHECK(installedBeta("0.3.0-beta.2") && !installedBeta("0.3.0"));

    CHECK(versionFromTag("v0.3.0-beta.2") == "0.3.0-beta.2");
    CHECK(refuses([] { versionFromTag("0.3.0"); }));
    CHECK(refuses([] { versionFromTag("v"); }));
    CHECK(refuses([] { versionFromTag("v1/2"); }));
    CHECK(refuses([] { versionFromTag("v.."); }));
    CHECK(refuses([] { assetName("bad/name"); }));

    // Stable follows the numeric version, never a prerelease.
    auto latest = Json::parse(releaseJson("v0.3.0", false, false, "2026-11-21T00:00:00Z", "stable notes"));
    auto stable = selectFromPage(latest, Channel::Stable);
    CHECK(stable && stable->version == "0.3.0" && stable->notes == "stable notes");
    CHECK(refuses([&] { selectFromPage(Json::parse(releaseJson("v0.4.0-beta.1", true, false, "2026-11-22T00:00:00Z")), Channel::Stable); }));
    CHECK(refuses([&] { selectFromPage(Json::parse(releaseJson("v0.3.0", false, true, "2026-11-21T00:00:00Z")), Channel::Stable); }));

    // Beta uses publication time, ignores stable releases and drafts.
    auto page = Json::parse("[" +
        releaseJson("v0.2.0", false, false, "2026-11-19T00:00:00Z") + "," +
        releaseJson("v0.4.0-beta.1", true, false, "2026-11-20T00:00:00Z") + "," +
        releaseJson("v0.3.1-beta.2", true, false, "2026-11-22T00:00:00Z") + "," +
        releaseJson("v0.9.0-beta.1", true, true, "2026-11-25T00:00:00Z") + "]");
    auto beta = selectFromPage(page, Channel::Beta);
    CHECK(beta && beta->version == "0.3.1-beta.2" && beta->published == "2026-11-22T00:00:00Z");
    CHECK(refuses([&] { candidate(Json::parse(releaseJson("v0.4.0-beta.1", false, false, "2026-11-22T00:00:00Z"))); }));
    CHECK(refuses([&] { candidate(Json::parse("{\"tag_name\":\"v1.2.3\"}")); }));
    CHECK(refuses([&] { candidate(Json::parse(releaseJson("v1.2.3", false, false, "2026-11-22 00:00:00"))); }));
    CHECK(refuses([&] { candidate(Json::parse("{}")); }));

    // A newer page replaces an earlier best candidate.
    auto old = selectFromPage(page, Channel::Beta);
    auto later = selectFromPage(Json::parse("[" + releaseJson("v0.3.2-beta.1", true, false, "2026-11-23T00:00:00Z") + "]"), Channel::Beta);
    CHECK(later && newer(*later, *old));

    // The selected release object owns the asset metadata.
    auto asset = assetName("0.3.1-beta.2");
    auto withAsset = Json::parse("{\"tag_name\":\"v0.3.1-beta.2\",\"prerelease\":true,\"draft\":false,"
        "\"published_at\":\"2026-11-22T00:00:00Z\",\"body\":\"notes\",\"assets\":["
        "{\"name\":\"SHA256SUMS\",\"size\":235},"
        "{\"name\":\"" + asset + "\",\"size\":40047725,\"digest\":\"sha256:" +
        std::string(64, 'a') + "\"}]}");
    CHECK(&releaseObject(withAsset, "v0.3.1-beta.2") == &withAsset);
    auto object = releaseObject(page, "v0.3.1-beta.2");
    CHECK(object["tag_name"] == "v0.3.1-beta.2");
    CHECK(refuses([&] { releaseObject(page, "v9.9.9"); }));

    Release release;
    release.version = "0.3.1-beta.2";
    selectAsset(release, withAsset);
    CHECK(release.asset == asset && release.size == 40047725 && release.explicitSize);
    CHECK(release.sha256 == std::string(64, 'a'));
    CHECK(refuses([&] {
        Release other;
        other.version = release.version;
        selectAsset(other, Json::parse("{\"assets\":[{\"name\":\"RiffRacer-Customs-other.zip\",\"size\":1}]}"));
    }));
    CHECK(refuses([&] {
        Release other;
        other.version = release.version;
        selectAsset(other, Json::parse("{\"assets\":[{\"name\":\"" + asset + "\",\"size\":1},"
                                       "{\"name\":\"" + asset + "\",\"size\":2}]}"));
    }));
    CHECK(refuses([&] {
        Release other;
        other.version = release.version;
        selectAsset(other, Json::parse("{\"assets\":[{\"name\":\"" + asset + "\",\"size\":0}]}"));
    }));
    CHECK(refuses([&] {
        Release other;
        other.version = release.version;
        selectAsset(other, Json::parse("{\"assets\":[{\"name\":\"" + asset + "\",\"size\":1,"
                                       "\"digest\":\"md5:abc\"}]}"));
    }));

    // Checksums must name the exact package once.
    auto digest = std::string(64, 'b');
    auto sums = digest + "  " + asset + "\r\n" + std::string(64, 'c') + "  SHA256SUMS\r\n";
    CHECK(checksumFor(sums, asset) == digest);
    CHECK(refuses([&] { checksumFor(std::string(64, 'b') + "  other.zip\n", asset); }));
    CHECK(refuses([&] { checksumFor("", asset); }));
    CHECK(refuses([&] { checksumFor(digest + "  " + asset + "\n" + digest + "  " + asset + "\n", asset); }));
    CHECK(refuses([&] { checksumFor(digest + "  " + asset + "\n" + std::string(64, 'd') + "  " + asset + "\n", asset); }));
    CHECK(refuses([&] { checksumFor("bad  " + asset + "\n", asset); }));
    CHECK(checksumFor(std::string(64, 'B') + " *" + asset + "\n", asset) == std::string(64, 'b'));

    // Channel state participates in the install decision.
    Release current;
    current.version = "0.3.0-beta.2";
    CHECK(!needsInstall(current, "0.3.0-beta.2", Channel::Beta, true));
    CHECK(needsInstall(current, "0.3.0-beta.2", Channel::Stable, true));
    CHECK(needsInstall(current, "0.3.0-beta.1", Channel::Beta, true));
    Release stableRelease;
    stableRelease.version = "0.3.0";
    CHECK(!needsInstall(stableRelease, "0.3.0", Channel::Stable, false));
    CHECK(needsInstall(stableRelease, "0.3.0", Channel::Beta, false));

    // Tags are encoded as one path component and never as a path.
    CHECK(encodeComponent("v0.3.0/beta#1") == "v0.3.0%2Fbeta%231");
    CHECK(refuses([] { encodeComponent(".."); }));
    CHECK(releaseUrl("v0.3.0", asset) ==
          std::string(downloadBase) + "/v0.3.0/" + asset);
    CHECK(sumsUrl("v0.3.0") == std::string(downloadBase) + "/v0.3.0/SHA256SUMS");

    std::cout << "PASS updater channel isolation, publication ordering, metadata, checksum and install rules\n";
    return 0;
}
