#pragma once
#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "gui/renderer.h"

namespace gui {

// A TrueType font drawn through the Renderer. Glyphs (printable ASCII and
// Latin-1) are rasterised with stb_truetype into a texture atlas the first time
// each pixel size is used. The pixel size is the requested size in points
// times the display's pixel scale, so text is rendered at the screen's real
// resolution and stays sharp on Retina displays.
class Font {
public:
    Font();
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool load(const char* path);
    void shutdown();  // frees the atlases; call while the GL context exists

    // `size` is the line height (ascent + descent) in points; (x, y) is the
    // top-left of the line box. Returns the x where the text ends.
    float draw(Renderer& r, std::string_view utf8, float x, float y, float size, Color c);
    float measure(const Renderer& r, std::string_view utf8, float size);

    // Distance from the top of the line box to the baseline, in points.
    float ascent(float size) const;

private:
    struct Atlas;
    struct Impl;

    Atlas& atlas(int pixelSize);

    std::unique_ptr<Impl> impl_;
    std::map<int, std::unique_ptr<Atlas>> atlases_;
};

}  // namespace gui
