# Chess

[![Build](https://github.com/alexander-aseme/chess-engine/actions/workflows/build.yml/badge.svg)](https://github.com/alexander-aseme/chess-engine/actions/workflows/build.yml)

A chess game and engine written from scratch in C++20, with an
OpenGL renderer on top of GLFW. Runs natively on macOS, Linux and Windows and in the
browser through WebAssembly.

![screenshot](docs/screenshot.png)

## Download

Ready-to-play builds for Windows, macOS and Linux are on the
[Releases](https://github.com/alexander-aseme/chess-engine/releases) page; nothing
needs to be installed. Unzip and run `chess` (`chess.exe` on Windows), keeping
the `assets` folder next to it.

- **macOS:** the build isn't signed by Apple, so the first launch is blocked.
  Open *System Settings → Privacy & Security* and click *Open Anyway*, or run
  `xattr -dr com.apple.quarantine chess-macos` once on the unpacked folder.
- **Windows:** SmartScreen may warn about an unrecognised app; choose
  *More info → Run anyway*.

Every push is built and tested on all three systems by
[GitHub Actions](.github/workflows/build.yml); pushing a tag such as `v0.3`
publishes a release.

## Build and run

### macOS (native)

```sh
xcode-select --install          # compiler, if you don't have it
brew install cmake

cmake -S . -B build
cmake --build build -j
./build/chess
```

CMake downloads GLFW, stb_image and miniaudio on the first configure, so you need an
internet connection the first time.

### Linux

```sh
sudo apt install build-essential cmake xorg-dev libwayland-dev libxkbcommon-dev
cmake -S . -B build
cmake --build build -j
./build/chess
```

### Windows

With Visual Studio 2022 or its Build Tools ("Desktop development with C++"), from a
Developer PowerShell:

```sh
cmake -S . -B build
cmake --build build --config Release
.\build\Release\chess.exe
```

Without admin rights, unzip [WinLibs](https://winlibs.com/) (GCC with CMake
included) anywhere in your user folder and put its `mingw64\bin` on the PATH for
the session:

```sh
$env:Path = "C:\path\to\mingw64\bin;$env:Path"
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --parallel
.\build\chess.exe
```

Either way the exe carries its own runtime, so `chess.exe` plus the `assets`
folder can be copied to any Windows PC.

The game opens on a menu: pick an opponent (two players on one device, or the
computer at one of four strengths, playing White, Black or a random colour) and
a time control, then Start. Drag a piece, or click it and then click a
square; promotions open a picker. Click a move in the list to jump to it.

| Key | Action |
|---|---|
| **←/→** | Step through moves (in untimed games, playing from an earlier move takes back the rest) |
| **↑/↓** (or Home/End) | First / latest move |
| **F** | Flip the board |
| **N** | Next game (colours swap, score kept) |
| **R** | Reset the match to 0-0 |
| **Esc** | Menu (pauses the clock) |

Settings (sound volume, flipping the board each turn) are saved in
`~/Library/Application Support/Chess/` on macOS, `~/.config/chess/` on Linux and
`%APPDATA%\Chess\` on Windows.

### Tests

```sh
cd build && ctest --output-on-failure     # or: ./build/engine_tests --deep
```

### Browser (WebAssembly)

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html), then:

```sh
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web
cmake --build build-web -j
python3 -m http.server -d build-web 8000   # then open http://localhost:8000/chess.html
```

The page has to be served over HTTP; opening the .html file directly won't load the assets.

## How it's built

```
src/
  engine/      pure C++ chess logic, no graphics (board, FEN, move generation, search)
  gui/         GLFW window, OpenGL renderer, the playable board view
  main.cpp     window/context setup and the frame loop (native + Emscripten)
tests/         FEN round-trip and perft tests
tools/         scripts that generate the piece sprite atlas and the sound effects
assets/        pieces.png
```

- **Engine and GUI are separate.** The engine never includes graphics code, so it
  can be tested headlessly and later exposed as a UCI engine.
- **One rendering path for desktop and web.** The renderer sticks to the subset
  shared by OpenGL 3.3 core and OpenGL ES 3.0 (WebGL 2); only the GLSL `#version`
  line differs (`src/gui/gl.h`).
- **Batched 2D renderer.** Quads are collected into one vertex buffer and drawn
  with a single call per texture. Textures use premultiplied alpha so scaled
  sprites have clean edges.
- **HiDPI aware.** Drawing happens in window points; the viewport uses the real
  framebuffer size, so it's sharp on Retina screens.

## Roadmap

- [x] Window, renderer, board and pieces, click-to-move, board flip
- [x] Board representation and FEN parsing, with tests
- [x] Legal move generation (castling, en passant, promotion), passing every perft test
- [x] Legal-move enforcement in the GUI
- [x] Undo/redo, drag-and-drop, promotion picker, check/checkmate display, sound effects
- [x] Text rendering (move list, coordinates, game result)
- [x] Evaluation: material + piece-square tables
- [x] Menu, settings, match score, chess clocks, insufficient-material draws
- [x] Search: negamax + alpha-beta, iterative deepening, quiescence, move ordering
- [x] Play against the computer (four strengths; search on a background thread)
- [x] Transposition table with Zobrist hashing
- [ ] Background search in the browser build too (it currently pauses the page while thinking)
- [ ] UCI mode: test against other engines and estimate Elo

## Credits

Piece sprites are rendered from the DejaVu Sans font (`tools/make_piece_atlas.py`).
The UI text uses DejaVu Sans too, trimmed to Latin-1 (license in `assets/fonts/LICENSE.txt`).
Sound effects are synthesized by `tools/make_sounds.py`.
GLFW (zlib license), stb_image (public domain / MIT) and miniaudio (public domain /
MIT-0) are downloaded at build time.
