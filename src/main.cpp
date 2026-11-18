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
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    double x, y;
    glfwGetCursorPos(w, &x, &y);  // window points, same space we draw in
    if (action == GLFW_PRESS) app->game.onMouseDown(float(x), float(y));
    if (action == GLFW_RELEASE) app->game.onMouseUp(float(x), float(y));
}

void onCursorPos(GLFWwindow* w, double x, double y) {
    static_cast<App*>(glfwGetWindowUserPointer(w))->game.onMouseMove(float(x), float(y));
}

void onScroll(GLFWwindow* w, double /*dx*/, double dy) {
    static_cast<App*>(glfwGetWindowUserPointer(w))->game.onScroll(float(dy));
}

void onKey(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action == GLFW_RELEASE) return;  // Esc opens the menu (handled by Game); close the window to quit
    static_cast<App*>(glfwGetWindowUserPointer(w))->game.onKey(key, action == GLFW_REPEAT);
}

// Draws and presents one frame at the window's current size.
void render(App* app) {
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(app->window, &winW, &winH);
    glfwGetFramebufferSize(app->window, &fbW, &fbH);

    app->renderer.beginFrame(winW, winH, fbW, fbH, {0.16f, 0.15f, 0.14f, 1.0f});
    app->game.draw(app->renderer, float(winW), float(winH));
    app->renderer.endFrame();

    glfwSwapBuffers(app->window);
}

// While the user drags a window edge, macOS (and Windows) run their own event loop,
// so glfwPollEvents() doesn't return and frame() can't run. GLFW still reports the
// size changes and redraw requests from inside that loop, so redraw from there too:
// the board then follows the edge live instead of jumping when the mouse is released.
void onResizeRedraw(GLFWwindow* w) { render(static_cast<App*>(glfwGetWindowUserPointer(w))); }
void onFramebufferSize(GLFWwindow* w, int /*width*/, int /*height*/) { onResizeRedraw(w); }

// One frame. Browsers don't allow an infinite loop on the main thread, so on
// the web Emscripten calls this once per animation frame instead.
void frame(void* userData) {
    auto* app = static_cast<App*>(userData);
    glfwPollEvents();
    render(app);
    if (app->game.quitRequested()) glfwSetWindowShouldClose(app->window, GLFW_TRUE);  // Quit in the menu
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
    app.window = glfwCreateWindow(1040, 760, "Chess", nullptr, nullptr);  // board + move panel
    if (!app.window) {
        glfwTerminate();
        return 1;
    }
    int minW, minH;
    gui::Game::minimumWindowSize(minW, minH);
    glfwSetWindowSizeLimits(app.window, minW, minH, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(app.window);

    #ifndef __EMSCRIPTEN__
        if (!gladLoadGL(glfwGetProcAddress)) {
            std::fprintf(stderr, "Failed to load OpenGL 3.3 functions\n");
            glfwTerminate();
            return 1;
        }
    #endif

    glfwSwapInterval(1);  // vsync
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetMouseButtonCallback(app.window, onMouseButton);
    glfwSetCursorPosCallback(app.window, onCursorPos);
    glfwSetScrollCallback(app.window, onScroll);
    glfwSetKeyCallback(app.window, onKey);

    std::printf("OpenGL: %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    if (!app.renderer.init() || !app.game.init()) {
        glfwTerminate();
        return 1;
    }
    // Only once the renderer and game exist, since these draw.
    glfwSetWindowRefreshCallback(app.window, onResizeRedraw);
    glfwSetFramebufferSizeCallback(app.window, onFramebufferSize);

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
