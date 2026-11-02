#include "gui/renderer.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace gui {

namespace {

const char* kVertexSrc = R"(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main() {
    // Window points (top-left origin) -> normalized device coordinates.
    vec2 ndc = aPos / uScreen * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vUV = aUV;
    vColor = vec4(aColor.rgb * aColor.a, aColor.a);  // premultiply
}
)";

const char* kFragmentSrc = R"(
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vUV) * vColor;
}
)";

GLuint compileShader(const GLenum type, const char* body) {
    const std::string src = std::string(CHESS_GLSL_HEADER) + body;
    const char* p = src.c_str();
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint uploadRGBA(const unsigned char* pixels, const int w, const int h, const bool mipmaps) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    return tex;
}

}  // namespace

Texture loadTexture(const char* path) {
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) {
        std::fprintf(stderr, "Could not load %s: %s\n", path, stbi_failure_reason());
        return {};
    }
    // Premultiply alpha so linear filtering blends edges correctly.
    for (int i = 0; i < w * h; ++i) {
        unsigned char* px = data + i * 4;
        for (int c = 0; c < 3; ++c) px[c] = (unsigned char)((px[c] * px[3] + 127) / 255);
    }
    Texture t{uploadRGBA(data, w, h, true), w, h};
    stbi_image_free(data);
    return t;
}

bool Renderer::init() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentSrc);
    if (!vs || !fs) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program_, sizeof log, nullptr, log);
        std::fprintf(stderr, "Shader link error:\n%s\n", log);
        return false;
    }
    uScreen_ = glGetUniformLocation(program_, "uScreen");
    uTex_ = glGetUniformLocation(program_, "uTex");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

    // 1x1 white texture so solid shapes use the same shader as sprites.
    const unsigned char white[4] = {255, 255, 255, 255};
    whiteTex_ = uploadRGBA(white, 1, 1, false);

    vertices_.reserve(6 * 1024);
    return true;
}

void Renderer::shutdown() {
    glDeleteTextures(1, &whiteTex_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
}

void Renderer::beginFrame(int windowW, int windowH, int fbW, int fbH, Color clear) {
    screenW_ = float(windowW > 0 ? windowW : 1);
    screenH_ = float(windowH > 0 ? windowH : 1);
    glViewport(0, 0, fbW, fbH);
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // premultiplied alpha
    glUseProgram(program_);
    glUniform2f(uScreen_, screenW_, screenH_);
    glUniform1i(uTex_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);
    currentTex_ = 0;
}

void Renderer::endFrame() { flush(); }

void Renderer::useTexture(GLuint tex) {
    if (tex != currentTex_) {
        flush();  // batch break: different texture
        currentTex_ = tex;
    }
}

void Renderer::pushVertex(float x, float y, float u, float v, Color c) {
    vertices_.push_back({x, y, u, v, c.r, c.g, c.b, c.a});
}

void Renderer::flush() {
    if (vertices_.empty()) return;
    glBindTexture(GL_TEXTURE_2D, currentTex_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices_.size()));
    vertices_.clear();
}

void Renderer::fillRect(Rect r, Color c) {
    useTexture(whiteTex_);
    float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    pushVertex(x0, y0, 0, 0, c);
    pushVertex(x1, y0, 1, 0, c);
    pushVertex(x1, y1, 1, 1, c);
    pushVertex(x0, y0, 0, 0, c);
    pushVertex(x1, y1, 1, 1, c);
    pushVertex(x0, y1, 0, 1, c);
}

void Renderer::drawCircle(float cx, float cy, float radius, Color c, int segments) {
    useTexture(whiteTex_);
    const float step = 6.2831853f / float(segments);
    for (int i = 0; i < segments; ++i) {
        float a0 = step * float(i), a1 = step * float(i + 1);
        pushVertex(cx, cy, 0.5f, 0.5f, c);
        pushVertex(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0.5f, 0.5f, c);
        pushVertex(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0.5f, 0.5f, c);
    }
}

void Renderer::drawSprite(const Texture& tex, Rect src, Rect dst, Color tint) {
    if (!tex.id) return;
    useTexture(tex.id);
    float u0 = src.x / float(tex.width), v0 = src.y / float(tex.height);
    float u1 = (src.x + src.w) / float(tex.width), v1 = (src.y + src.h) / float(tex.height);
    float x0 = dst.x, y0 = dst.y, x1 = dst.x + dst.w, y1 = dst.y + dst.h;
    pushVertex(x0, y0, u0, v0, tint);
    pushVertex(x1, y0, u1, v0, tint);
    pushVertex(x1, y1, u1, v1, tint);
    pushVertex(x0, y0, u0, v0, tint);
    pushVertex(x1, y1, u1, v1, tint);
    pushVertex(x0, y1, u0, v1, tint);
}

}  // namespace gui
