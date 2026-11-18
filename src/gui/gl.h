#pragma once
// One place that decides where OpenGL comes from on each platform.
//
// We write to the subset shared by desktop OpenGL 3.3 core and OpenGL ES 3.0
// (= WebGL 2), so the same rendering code runs natively and in the browser.

#if defined(__EMSCRIPTEN__)
    #include <GLES3/gl3.h>
    #define CHESS_GLSL_HEADER "#version 300 es\nprecision mediump float;\n"
#else
    // Desktop: glad loads GL 3.3 core functions from the driver at runtime
    // (gladLoadGL after the context is created). Its header declares only
    // GL 3.3 core, so anything newer fails to compile.
    #include <glad/gl.h>
    #define CHESS_GLSL_HEADER "#version 330 core\n"
#endif

#define GLFW_INCLUDE_NONE  // we include GL ourselves, above
#include <GLFW/glfw3.h>
