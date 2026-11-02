#pragma once
#include <vector>

#include "gui/gl.h"

namespace gui {

struct Color {
    float r, g, b, a;
};

struct Rect {
    float x, y, w, h;
};

// A texture uploaded to the GPU. Pixels are stored premultiplied by alpha,
// which avoids dark fringes around sprite edges when they are scaled.
struct Texture {
    GLuint id = 0;
    int width = 0, height = 0;
};

// Loads a PNG (via stb_image) and uploads it with mipmaps. Returns id 0 on failure.
Texture loadTexture(const char* path);

// Minimal 2D batch renderer: collects coloured / textured quads into one
// vertex buffer and draws them with as few draw calls as possible.
// Coordinates are in window points with (0, 0) at the top-left.
class Renderer {
public:
    bool init();
    void shutdown();

    // windowW/H: window size.
    // fbW/fbH:   framebuffer size in pixels (2x on a Retina display).
    void beginFrame(int windowW, int windowH, int fbW, int fbH, Color clear);
    void endFrame();

    void fillRect(Rect r, Color c);
    void drawCircle(float cx, float cy, float radius, Color c, int segments = 32);
    // Draws the sub-rectangle `src` (in texture pixels) of `tex` into `dst`.
    void drawSprite(const Texture& tex, Rect src, Rect dst, Color tint = {1, 1, 1, 1});

private:
    struct Vertex {
        float x, y, u, v, r, g, b, a;
    };

    void useTexture(GLuint tex);
    void pushVertex(float x, float y, float u, float v, Color c);
    void flush();

    GLuint program_ = 0, vao_ = 0, vbo_ = 0, whiteTex_ = 0;
    GLint uScreen_ = -1, uTex_ = -1;
    GLuint currentTex_ = 0;
    float screenW_ = 1, screenH_ = 1;
    std::vector<Vertex> vertices_;
};

}  // namespace gui
