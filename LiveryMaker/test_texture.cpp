// Texture authoring: PNG decode, resize, DXT payloads and TABT atlas rebuild.
#include "texture_encode.hpp"
#include <iostream>

using namespace livery;
using namespace livery::textures;

#define CHECK(condition) do {if (!(condition)) {std::cerr << "FAIL " << __LINE__ << ": " #condition "\n"; return 1;}} while (0)

static Image gradient(unsigned width, unsigned height, bool alpha) {
    Image image{width, height, Bytes(size_t(width) * height * 4)};
    for (unsigned y = 0; y < height; y++)
        for (unsigned x = 0; x < width; x++) {
            size_t at = (size_t(y) * width + x) * 4;
            image.pixels[at] = uint8_t(x * 255 / std::max(1u, width - 1));
            image.pixels[at + 1] = uint8_t(y * 255 / std::max(1u, height - 1));
            image.pixels[at + 2] = uint8_t((x + y) * 127 / std::max(1u, width + height - 2));
            image.pixels[at + 3] = alpha ? uint8_t(64 + (x * 191 / std::max(1u, width - 1))) : 255;
        }
    return image;
}

static Texture texture(const std::string &name, const Image &image, const char *fourcc) {
    auto dds = buildDDS(image, fourcc);
    Texture result;
    result.name = name;
    result.fourcc = fourcc;
    result.width = image.width;
    result.height = image.height;
    result.flags = 0;
    result.mips = u32(dds, 28);
    result.caps = u32(dds, 112);
    result.dds = std::move(dds);
    return result;
}

int main() {
    auto source = gradient(64, 64, true);
    auto encodedDxt5 = texture("gradient", source, "DXT5");
    CHECK(encodedDxt5.mips == 7 && encodedDxt5.caps == 0);
    auto decoded = decodeDDS(encodedDxt5);
    CHECK(decoded.width == 64 && decoded.height == 64);
    long long error = 0, alphaError = 0;
    for (size_t i = 0; i < source.pixels.size(); i += 4) {
        for (int c = 0; c < 3; c++) error += std::abs(int(decoded.pixels[i + c]) - int(source.pixels[i + c]));
        alphaError += std::abs(int(decoded.pixels[i + 3]) - int(source.pixels[i + 3]));
    }
    CHECK(error / (64 * 64 * 3) < 14);
    CHECK(alphaError / (64 * 64) < 10);

    auto opaque = gradient(32, 32, false);
    auto encodedDxt1 = texture("flat", opaque, "DXT1");
    CHECK(encodedDxt1.mips == 6 && encodedDxt1.dds.size() == 128 + 8 * (64 + 16 + 4 + 1 + 1 + 1));
    auto flat = decodeDDS(encodedDxt1);
    long long flatError = 0;
    for (size_t i = 0; i < opaque.pixels.size(); i += 4)
        for (int c = 0; c < 3; c++) flatError += std::abs(int(flat.pixels[i + c]) - int(opaque.pixels[i + c]));
    CHECK(flatError / (32 * 32 * 3) < 16);

    // Non-power-of-two source images resize to the requested native size.
    auto resized = resize(gradient(100, 60, false), 64, 32);
    CHECK(resized.width == 64 && resized.height == 32 && resized.pixels.size() == 64 * 32 * 4);
    auto blocky = resize(gradient(64, 64, false), 16, 16);
    CHECK(blocky.width == 16 && blocky.height == 16);
    CHECK(powerOfTwo(2048, 64, 1024) == 1024 && powerOfTwo(512, 64, 1024) == 512 && powerOfTwo(100, 64, 1024) == 64);

    // A TABT atlas round-trips names, dimensions, flags and texture payloads.
    auto atlas = buildAtlas({encodedDxt5, encodedDxt1});
    auto textures = decodeTextures(atlas);
    CHECK(textures.size() == 2 && textures[0].name == "gradient" && textures[1].name == "flat");
    CHECK(textures[0].width == 64 && textures[0].mips == 7);
    CHECK(textures[1].fourcc == "DXT1" && textures[1].width == 32);
    CHECK(decodeDDS(textures[0]).pixels == decoded.pixels);

    auto replaced = withTexture(atlas, texture("flat", gradient(16, 16, false), "DXT1"));
    auto after = decodeTextures(replaced);
    CHECK(after.size() == 2 && after[1].width == 16);
    auto appended = withTexture(replaced, texture("added", gradient(8, 8, false), "DXT1"));
    CHECK(decodeTextures(appended).size() == 3);

    // PNG memory decoding accepts what the editor writes.
    auto pngPath = fs::temp_directory_path() / "customs-texture-test.png";
    savePNG(pngPath, source);
    auto png = read(pngPath.u8string());
    fs::remove(pngPath);
    auto decodedPng = decodePNG(png);
    CHECK(decodedPng.width == source.width && decodedPng.height == source.height);
    CHECK(decodedPng.pixels == source.pixels);

    std::cout << "PASS texture PNG decode, resize, DXT1/DXT5 mips, DDS and TABT atlas rebuild\n";
    return 0;
}
