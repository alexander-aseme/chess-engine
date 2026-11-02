// Entry point: creates the window and GL context, then runs the frame loop.
// The same code builds natively (macOS/Linux) and for the browser (Emscripten).

#include <cstdio>

#include "gui/game.h"
#include "gui/gl.h"
#include "gui/renderer.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

struct App {
    GLFWwindow* window = nullptr;
    gui::Renderer renderer;
    gui::Game game;
};

void onMouseButton(GLFWwindow* w, int button, int action, int /*mods*/) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    double x, y;
    glfwGetCursorPos(w, &x, &y);  // window points, same space we draw in
    app->game.onMouseClick(float(x), float(y));
}

void onKey(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
    static_cast<App*>(glfwGetWindowUserPointer(w))->game.onKey(key);
}

// One frame. Browsers don't allow an infinite loop on the main thread, so on
// the web Emscripten calls this once per animation frame instead.
void frame(void* userData) {
    auto* app = static_cast<App*>(userData);
    glfwPollEvents();

    int winW, winH, fbW, fbH;
    glfwGetWindowSize(app->window, &winW, &winH);
    glfwGetFramebufferSize(app->window, &fbW, &fbH);

    app->renderer.beginFrame(winW, winH, fbW, fbH, {0.16f, 0.15f, 0.14f, 1.0f});
    app->game.draw(app->renderer, float(winW), float(winH));
    app->renderer.endFrame();

    glfwSwapBuffers(app->window);
}

}  // namespace

int main() {
    glfwSetErrorCallback([](int code, const char* msg) {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, msg);
    });
    if (!glfwInit()) return 1;

#ifdef __EMSCRIPTEN__
    // WebGL 2 == OpenGL ES 3.0
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    // macOS only hands out a modern context if you ask for core + forward-compat.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);  // MSAA for smooth circle edges

    static App app;  // static: must outlive main() on the web
    app.window = glfwCreateWindow(760, 760, "Chess", nullptr, nullptr);
    if (!app.window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(app.window);
    glfwSwapInterval(1);  // vsync
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetMouseButtonCallback(app.window, onMouseButton);
    glfwSetKeyCallback(app.window, onKey);

    std::printf("OpenGL: %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    if (!app.renderer.init() || !app.game.init()) {
        glfwTerminate();
        return 1;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, &app, 0, true);  // never returns
#else
    while (!glfwWindowShouldClose(app.window)) frame(&app);
    app.game.shutdown();
    app.renderer.shutdown();
    glfwDestroyWindow(app.window);
    glfwTerminate();
#endif
    return 0;
}
