#include "gui/font.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace gui {

namespace {

// Codepoint ranges baked into each atlas: printable ASCII and Latin-1 (which
// includes ½ for drawn results and × for the eval sign, among others).
struct Range {
    int first, count;
};
constexpr Range kRanges[] = {{32, 95}, {160, 96}};
constexpr int kGlyphCount = 95 + 96;

// Index of codepoint cp in the baked glyph list, or -1 if it isn't baked.
int glyphIndex(uint32_t cp) {
    int base = 0;
    for (const Range& r : kRanges) {
        if (cp >= uint32_t(r.first) && cp < uint32_t(r.first + r.count)) return base + int(cp) - r.first;
        base += r.count;
    }
    return -1;
}

// Decodes the next UTF-8 codepoint from s at i and advances i. Malformed bytes
// decode as '?'.
uint32_t nextCodepoint(std::string_view s, size_t& i) {
    auto byte = [&](size_t k) { return k < s.size() ? uint8_t(s[k]) : 0u; };
    uint32_t c = byte(i);
    int extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
    if (extra < 0) {
        ++i;
        return '?';
    }
    uint32_t cp = extra == 0 ? c : c & (0x3F >> extra);
    for (int k = 1; k <= extra; ++k) {
        uint32_t b = byte(i + k);
        if ((b & 0xC0) != 0x80) {
            i += k;
            return '?';
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    i += 1 + extra;
    return cp;
}

}  // namespace

struct Font::Impl {
    std::vector<unsigned char> ttf;  // stb_truetype reads glyphs from here, so it must stay alive
    stbtt_fontinfo info{};
    int ascent = 0, descent = 0;  // font units
};

struct Font::Atlas {
    Texture texture;
    stbtt_packedchar glyphs[kGlyphCount]{};
    float kernScale = 0;  // font units -> pixels at this size
};

Font::Font() : impl_(std::make_unique<Impl>()) {}
Font::~Font() = default;

bool Font::load(const char* path) {
    std::ifstream in(path, std::ios::binary);
    impl_->ttf.assign(std::istreambuf_iterator<char>(in), {});
    if (impl_->ttf.empty() ||
        !stbtt_InitFont(&impl_->info, impl_->ttf.data(), stbtt_GetFontOffsetForIndex(impl_->ttf.data(), 0))) {
        std::fprintf(stderr, "Could not load font %s\n", path);
        return false;
    }
    int lineGap = 0;
    stbtt_GetFontVMetrics(&impl_->info, &impl_->ascent, &impl_->descent, &lineGap);
    return true;
}

void Font::shutdown() {
    for (auto& [size, a] : atlases_) glDeleteTextures(1, &a->texture.id);
    atlases_.clear();
}

float Font::ascent(float size) const {
    return size * float(impl_->ascent) / float(impl_->ascent - impl_->descent);
}

Font::Atlas& Font::atlas(int pixelSize) {
    auto& slot = atlases_[pixelSize];
    if (slot) return *slot;
    slot = std::make_unique<Atlas>();
    Atlas& a = *slot;
    a.kernScale = stbtt_ScaleForPixelHeight(&impl_->info, float(pixelSize));

    // Pack into the smallest power-of-two square that fits, growing as needed.
    for (int dim = 256; dim <= 4096; dim *= 2) {
        std::vector<unsigned char> alpha(size_t(dim) * dim);
        stbtt_pack_context pc;
        if (!stbtt_PackBegin(&pc, alpha.data(), dim, dim, 0, 1, nullptr)) break;
        bool ok = true;
        int base = 0;
        for (const Range& r : kRanges) {
            ok = ok && stbtt_PackFontRange(&pc, impl_->ttf.data(), 0, float(pixelSize), r.first, r.count,
                                           a.glyphs + base);
            base += r.count;
        }
        stbtt_PackEnd(&pc);
        if (!ok) continue;

        // White glyphs with coverage in alpha, premultiplied: every channel = coverage.
        std::vector<unsigned char> rgba(alpha.size() * 4);
        for (size_t i = 0; i < alpha.size(); ++i)
            rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = rgba[i * 4 + 3] = alpha[i];
        a.texture = createTexture(rgba.data(), dim, dim, false);
        break;
    }
    if (!a.texture.id) std::fprintf(stderr, "Could not bake font at %d px\n", pixelSize);
    return a;
}

// Both draw() and measure() walk the string the same way, so measured widths
// match what's drawn: lay out in whole pixels at the baked size, then convert
// back to points.
float Font::draw(Renderer& r, std::string_view utf8, float x, float y, float size, Color c) {
    float scale = r.pixelScale();
    int px = std::max(1, int(std::lround(size * scale)));
    Atlas& a = atlas(px);

    // Snap the pen and baseline to whole pixels so glyphs map 1:1 onto the screen.
    float penX = std::round(x * scale);
    float baseline = std::round((y + ascent(size)) * scale);
    uint32_t prev = 0;
    for (size_t i = 0; i < utf8.size();) {
        uint32_t cp = nextCodepoint(utf8, i);
        int g = glyphIndex(cp);
        if (g < 0) g = glyphIndex('?');
        if (prev) penX += std::round(a.kernScale * float(stbtt_GetCodepointKernAdvance(&impl_->info, int(prev), int(cp))));

        stbtt_aligned_quad q;
        float qx = penX, qy = baseline;
        stbtt_GetPackedQuad(a.glyphs, a.texture.width, a.texture.height, g, &qx, &qy, &q, 1);
        penX += std::round(a.glyphs[g].xadvance);  // same rounding as measure()
        if (q.x1 > q.x0) {
            Rect src{q.s0 * float(a.texture.width), q.t0 * float(a.texture.height),
                     (q.s1 - q.s0) * float(a.texture.width), (q.t1 - q.t0) * float(a.texture.height)};
            Rect dst{q.x0 / scale, q.y0 / scale, (q.x1 - q.x0) / scale, (q.y1 - q.y0) / scale};
            r.drawSprite(a.texture, src, dst, c);
        }
        prev = cp;
    }
    return penX / scale;
}

float Font::measure(const Renderer& r, std::string_view utf8, float size) {
    float scale = r.pixelScale();
    int px = std::max(1, int(std::lround(size * scale)));
    Atlas& a = atlas(px);

    float penX = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < utf8.size();) {
        uint32_t cp = nextCodepoint(utf8, i);
        int g = glyphIndex(cp);
        if (g < 0) g = glyphIndex('?');
        if (prev) penX += std::round(a.kernScale * float(stbtt_GetCodepointKernAdvance(&impl_->info, int(prev), int(cp))));
        penX += std::round(a.glyphs[g].xadvance);
        prev = cp;
    }
    return penX / scale;
}

}  // namespace gui
