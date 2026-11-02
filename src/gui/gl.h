#pragma once
// One place that decides where OpenGL comes from on each platform.
//
// We write to the subset shared by desktop OpenGL 3.3 core and OpenGL ES 3.0
// (= WebGL 2), so the same rendering code runs natively and in the browser.

#if defined(__EMSCRIPTEN__)
    #include <GLES3/gl3.h>
    #define CHESS_GLSL_HEADER "#version 300 es\nprecision mediump float;\n"
#elif defined(__APPLE__)
    #define GL_SILENCE_DEPRECATION  // Apple deprecated GL in 2018; it still works (up to 4.1)
    #include <OpenGL/gl3.h>
    #define CHESS_GLSL_HEADER "#version 330 core\n"
#elif defined(__linux__)
    #define GL_GLEXT_PROTOTYPES     // Mesa exports core GL functions directly
    #include <GL/glcorearb.h>
    #define CHESS_GLSL_HEADER "#version 330 core\n"
#else
    // Windows needs a function loader for anything newer than GL 1.1.
    // Generate one at https://gen.glad.sh (GL 3.3 core) and include it here.
    #error "Add a GL loader (e.g. glad) for this platform"
#endif

#define GLFW_INCLUDE_NONE  // we include GL ourselves, above
#include <GLFW/glfw3.h>
