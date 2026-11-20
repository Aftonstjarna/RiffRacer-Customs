#pragma once
// Texture authoring helpers for imported vehicle materials.
// The game stores textures in a TABT v4 container holding DDS payloads.
// This file converts source images into that form and rebuilds an atlas.
#include "core.hpp"

namespace livery {
namespace textures {

inline Image decodePNG(const Bytes &encoded) {
    require(!encoded.empty() && encoded.size() <= 256ULL * 1024 * 1024, "PNG exceeds 256 MiB");
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    require(png_image_begin_read_from_memory(&png, encoded.data(), encoded.size()) != 0,
            "Cannot read PNG header");
    require(png.width > 0 && png.height > 0 && png.width <= 8192 && png.height <= 8192,
            "Unsupported PNG dimensions");
    png.format = PNG_FORMAT_RGBA;
    Image out{png.width, png.height, Bytes(size_t(png.width) * png.height * 4)};
    bool ok = png_image_finish_read(&png, nullptr, out.pixels.data(), 0, nullptr) != 0;
    png_image_free(&png);
    require(ok, "Invalid or truncated PNG");
    return out;
}

inline Image resize(const Image &source, unsigned width, unsigned height) {
    require(source.width && source.height && width && height, "Invalid image size");
    require(width <= 4096 && height <= 4096, "Image exceeds 4096 px");
    if (source.width == width && source.height == height) return source;
    Image out{width, height, Bytes(size_t(width) * height * 4)};
    bool integral = source.width % width == 0 && source.height % height == 0;
    if (integral) {
        unsigned bx = source.width / width, by = source.height / height;
        for (unsigned y = 0; y < height; y++)
            for (unsigned x = 0; x < width; x++)
                for (int c = 0; c < 4; c++) {
                    unsigned sum = 0;
                    for (unsigned dy = 0; dy < by; dy++)
                        for (unsigned dx = 0; dx < bx; dx++)
                            sum += source.pixels[((size_t(y) * by + dy) * source.width + x * bx + dx) * 4 + c];
                    out.pixels[(size_t(y) * width + x) * 4 + c] = uint8_t((sum + bx * by / 2) / (bx * by));
                }
        return out;
    }
    for (unsigned y = 0; y < height; y++)
        for (unsigned x = 0; x < width; x++) {
            double sx = (x + .5) * source.width / width - .5;
            double sy = (y + .5) * source.height / height - .5;
            unsigned x0 = unsigned(std::max(0., std::floor(sx))), y0 = unsigned(std::max(0., std::floor(sy)));
            unsigned x1 = std::min(source.width - 1, x0 + 1), y1 = std::min(source.height - 1, y0 + 1);
            double fx = std::clamp(sx - x0, 0., 1.), fy = std::clamp(sy - y0, 0., 1.);
            for (int c = 0; c < 4; c++) {
                double a = source.pixels[(size_t(y0) * source.width + x0) * 4 + c];
                double b = source.pixels[(size_t(y0) * source.width + x1) * 4 + c];
                double d = source.pixels[(size_t(y1) * source.width + x0) * 4 + c];
                double e = source.pixels[(size_t(y1) * source.width + x1) * 4 + c];
                double top = a + (b - a) * fx, bottom = d + (e - d) * fx;
                out.pixels[(size_t(y) * width + x) * 4 + c] = uint8_t(std::clamp(top + (bottom - top) * fy, 0., 255.) + .5);
            }
        }
    return out;
}

inline unsigned powerOfTwo(unsigned value, unsigned minimum, unsigned maximum) {
    unsigned result = minimum;
    while (result < maximum && result * 2 <= value) result *= 2;
    return result;
}

inline bool hasAlpha(const Image &image) {
    for (size_t i = 3; i < image.pixels.size(); i += 4)
        if (image.pixels[i] != 255) return true;
    return false;
}

inline Bytes buildDDS(const Image &source, const char *fourcc, unsigned levelLimit = 0) {
    require(source.width && source.height, "Empty texture");
    bool bc1 = std::string(fourcc) == "DXT1";
    bool bc3 = std::string(fourcc) == "DXT5";
    require(bc1 || bc3, "Only DXT1 and DXT5 are supported for imported textures");
    unsigned levels = 1;
    for (unsigned size = std::max(source.width, source.height); size > 1; size >>= 1) levels++;
    if (levelLimit) levels = std::min(levels, levelLimit);
    auto levelBytes = [&](unsigned w, unsigned h) {
        return size_t((w + 3) / 4) * ((h + 3) / 4) * (bc1 ? 8 : 16);
    };
    Bytes out(128);
    std::memcpy(out.data(), "DDS ", 4);
    set32(out, 4, 124);
    set32(out, 8, 0x000A1007);  // caps | height | width | pixel format | linear size | mip count
    set32(out, 12, source.height);
    set32(out, 16, source.width);
    set32(out, 20, uint32_t(levelBytes(source.width, source.height)));
    set32(out, 24, 0);
    set32(out, 28, levels);
    set32(out, 76, 32);
    set32(out, 80, 0x4);  // DDPF_FOURCC
    std::memcpy(out.data() + 84, fourcc, 4);
    set32(out, 108, 0x1000 | (levels > 1 ? 0x400008 : 0));  // texture, complex + mipmap
    Image level = source;
    for (unsigned i = 0; i < levels; i++) {
        auto block = encodeLevel(level, false, bc1);
        out.insert(out.end(), block.begin(), block.end());
        if (i + 1 < levels) level = downsample(level);
    }
    return out;
}

inline std::vector<Texture> decodeAtlas(const Bytes &bank) { return decodeTextures(bank); }

inline Bytes buildAtlas(const std::vector<Texture> &textures) {
    require(!textures.empty() && textures.size() < 65536, "Invalid texture count");
    Bytes names;
    std::vector<uint32_t> nameOffsets;
    for (const auto &texture : textures) {
        require(!texture.name.empty() && texture.name.size() < 128, "Invalid texture name");
        nameOffsets.push_back(uint32_t(names.size()));
        names.insert(names.end(), texture.name.begin(), texture.name.end());
        names.push_back(0);
        require(texture.dds.size() >= 128 && std::memcmp(texture.dds.data(), "DDS ", 4) == 0,
                "Invalid DDS payload");
        require(texture.dds.size() <= 16 * 1024 * 1024, "Texture exceeds 16 MiB");
    }
    // Keep the payload 32-byte aligned like the original authoring tooling.
    while ((28 + textures.size() * 24 + names.size()) % 32) names.push_back(0);
    uint64_t payloadSize = 0;
    for (const auto &texture : textures) payloadSize += texture.dds.size();
    require(payloadSize < 0xffffffffULL, "Texture atlas exceeds 4 GiB");
    uint32_t payloadStart = uint32_t(28 + textures.size() * 24 + names.size());
    Bytes out;
    auto u32v = [&](uint32_t v) { put32(out, v); };
    out.insert(out.end(), {'T', 'A', 'B', 'T'});
    u32v(0xffee);
    u32v(4);
    u32v(payloadStart);
    u32v(uint32_t(textures.size()));
    u32v(uint32_t(names.size()));
    u32v(uint32_t(payloadSize));
    uint32_t offset = 0;
    for (size_t i = 0; i < textures.size(); i++) {
        const auto &texture = textures[i];
        u32v(nameOffsets[i]);
        u32v(offset);
        u32v(uint32_t(texture.dds.size()));
        u32v(texture.width);
        u32v(texture.height);
        u32v(texture.flags);
        offset += uint32_t(texture.dds.size());
    }
    out.insert(out.end(), names.begin(), names.end());
    for (const auto &texture : textures) out.insert(out.end(), texture.dds.begin(), texture.dds.end());
    require(out.size() == payloadStart + payloadSize, "Atlas size mismatch");
    return out;
}

// Replace a texture in place, or append it when the name is new.
inline Bytes withTexture(const Bytes &original, const Texture &texture) {
    auto all = decodeTextures(original);
    bool replaced = false;
    for (auto &entry : all)
        if (entry.name == texture.name) {
            require(!replaced, "Duplicate texture name in atlas");
            entry = texture;
            replaced = true;
        }
    if (!replaced) all.push_back(texture);
    return buildAtlas(all);
}

} // namespace textures
} // namespace livery
