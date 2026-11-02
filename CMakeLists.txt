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
# Engine: pure C++, no graphics. Shared by the game, the tests and (later) a
# UCI command-line engine.
# ---------------------------------------------------------------------------
add_library(engine STATIC
  src/engine/board.cpp
  src/engine/movegen.cpp
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

  add_executable(chess
    src/main.cpp
    src/gui/renderer.cpp
    src/gui/game.cpp
  )
  target_link_libraries(chess PRIVATE engine)
  target_include_directories(chess PRIVATE src ${stb_SOURCE_DIR})

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

    if(APPLE)
      target_link_libraries(chess PRIVATE "-framework OpenGL")
    else()
      find_package(OpenGL REQUIRED)
      target_link_libraries(chess PRIVATE OpenGL::GL)
    endif()

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
