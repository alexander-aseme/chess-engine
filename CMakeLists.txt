cmake_minimum_required(VERSION 3.20)
project(chess LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # helps editors/clangd find headers

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CHESS_BUILD_GUI "Build the graphical game" ON)
option(CHESS_BUILD_TESTS "Build engine tests" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Engine: chess rules (board, FEN, move generation) with no graphics
# dependency. Linked into both the game and the tests.
# ---------------------------------------------------------------------------
add_library(engine STATIC
  src/engine/board.cpp
  src/engine/movegen.cpp
  src/engine/eval.cpp
  src/engine/notation.cpp
  src/engine/search.cpp
)
target_include_directories(engine PUBLIC src)
if(MSVC)
  target_compile_options(engine PRIVATE /W4)
else()
  target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Game: GLFW window + OpenGL 3.3 core / OpenGL ES 3.0 (WebGL 2).
# ---------------------------------------------------------------------------
if(CHESS_BUILD_GUI)
  # stb_image (single header PNG loader). No CMakeLists of its own, so this
  # just downloads it into the build folder.
  FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(stb)

  # miniaudio (single header audio playback; Web Audio under Emscripten).
  # SOURCE_SUBDIR points at a folder that doesn't exist so only the header is
  # downloaded and none of its own CMake targets are built.
  FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  header-only)
  FetchContent_MakeAvailable(miniaudio)

  add_executable(chess
    src/main.cpp
    src/gui/renderer.cpp
    src/gui/game.cpp
    src/gui/audio.cpp
    src/gui/font.cpp
    src/gui/settings.cpp
    src/gui/ui.cpp
  )
  target_link_libraries(chess PRIVATE engine)
  target_include_directories(chess PRIVATE src ${stb_SOURCE_DIR} ${miniaudio_SOURCE_DIR})

  if(EMSCRIPTEN)
    # Emscripten ships its own GLFW implementation on top of the browser canvas.
    set_target_properties(chess PROPERTIES SUFFIX ".html")
    target_compile_options(chess PRIVATE -sUSE_GLFW=3)
    target_link_options(chess PRIVATE
      -sUSE_GLFW=3
      -sMIN_WEBGL_VERSION=2
      -sMAX_WEBGL_VERSION=2
      -sALLOW_MEMORY_GROWTH=1
      --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets)
  else()
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG        3.4
      GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glfw)
    target_link_libraries(chess PRIVATE glfw)

    # glad: generated GL 3.3 core loader (gen.glad.sh), committed in external/.
    # Desktop only; Emscripten provides GLES3 itself.
    add_library(glad STATIC external/glad/src/gl.c)
    target_include_directories(glad PUBLIC external/glad/include)
    target_link_libraries(chess PRIVATE glad)

    if(APPLE)
      target_link_libraries(chess PRIVATE
        "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
    elseif(UNIX)
      # miniaudio needs threads, dlopen (for audio backends) and libm.
      find_package(Threads REQUIRED)
      target_link_libraries(chess PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
    endif()
    # No OpenGL link on any platform: GLFW loads the GL library at runtime and
    # glad fetches every function through glfwGetProcAddress.

    # Copy assets next to the executable so it runs from the build folder.
    add_custom_command(TARGET chess POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:chess>/assets)
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests (native only): FEN parsing and perft.
# ---------------------------------------------------------------------------
if(CHESS_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_executable(engine_tests tests/engine_tests.cpp)
  target_link_libraries(engine_tests PRIVATE engine)
  add_test(NAME engine_tests COMMAND engine_tests)
endif()
