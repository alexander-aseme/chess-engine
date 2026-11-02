#include "gui/game.h"

#include <algorithm>
#include <cstdio>

namespace gui {

using namespace chess;

namespace {

// Flip to true once generateLegalMoves() works. Until then, any piece of the
// side to move can go to any square, so you can test the GUI end to end.
constexpr bool kEnforceLegalMoves = false;

constexpr Color kLightSquare{.r = 0.93f, .g = 0.85f, .b = 0.71f, .a = 1.0f};
constexpr Color kDarkSquare{.r = 0.71f, .g = 0.53f, .b = 0.39f, .a = 1.0f};
constexpr Color kSelected{.r = 0.98f, .g = 0.85f, .b = 0.25f, .a = 0.55f};
constexpr Color kLastMove{.r = 0.80f, .g = 0.82f, .b = 0.30f, .a = 0.40f};
constexpr Color kMoveDot{.r = 0.10f, .g = 0.10f, .b = 0.10f, .a = 0.28f};

constexpr int kSpriteCell = 128;  // must match tools/make_piece_atlas.py

}  // namespace

bool Game::init() {
    pieces_ = loadTexture("assets/pieces.png");
    if (!pieces_.id) {
        std::fprintf(stderr, "Run the game from the project or build folder so "
                             "assets/pieces.png can be found.\n");
        return false;
    }
    return true;
}

void Game::shutdown() { glDeleteTextures(1, &pieces_.id); }

Rect Game::squareRect(Square s) const {
    int file = fileOf(s), rank = rankOf(s);
    int col = flipped_ ? 7 - file : file;
    int row = flipped_ ? rank : 7 - rank;  // row 0 is the top of the screen
    return {.x = boardRect_.x + col * squareSize_, .y = boardRect_.y + row * squareSize_,
            .w = squareSize_, .h = squareSize_};
}

Square Game::squareAt(float x, float y) const {
    float lx = x - boardRect_.x, ly = y - boardRect_.y;
    if (lx < 0 || ly < 0 || lx >= boardRect_.w || ly >= boardRect_.h) return NoSquare;
    int col = int(lx / squareSize_), row = int(ly / squareSize_);
    int file = flipped_ ? 7 - col : col;
    int rank = flipped_ ? row : 7 - row;
    return makeSquare(file, rank);
}

void Game::draw(Renderer& r, float windowW, float windowH) {
    // Largest square board that fits, with a small margin, centred.
    float margin = 24.0f;
    float size = std::max(64.0f, std::min(windowW, windowH) - 2 * margin);
    squareSize_ = size / 8.0f;
    boardRect_ = {(windowW - size) / 2, (windowH - size) / 2, size, size};

    for (Square s = 0; s < 64; ++s) {
        bool light = (fileOf(s) + rankOf(s)) % 2 == 1;
        r.fillRect(squareRect(s), light ? kLightSquare : kDarkSquare);
    }

    if (lastMove_.from != NoSquare) {
        r.fillRect(squareRect(lastMove_.from), kLastMove);
        r.fillRect(squareRect(lastMove_.to), kLastMove);
    }
    if (selected_ != NoSquare) r.fillRect(squareRect(selected_), kSelected);

    for (Square s = 0; s < 64; ++s) {
        Piece p = board_.at(s);
        if (p == NoPiece) continue;
        Rect src{float(typeOf(p) * kSpriteCell), float(colorOf(p) * kSpriteCell),
                 float(kSpriteCell), float(kSpriteCell)};
        r.drawSprite(pieces_, src, squareRect(s));
    }

    for (const Move& m : selectedMoves_) {
        Rect sq = squareRect(m.to);
        r.drawCircle(sq.x + sq.w / 2, sq.y + sq.h / 2, sq.w * 0.16f, kMoveDot);
    }
}

void Game::select(Square s) {
    selected_ = s;
    selectedMoves_.clear();
    if (s == NoSquare) return;
    for (const Move& m : generateLegalMoves(board_))
        if (m.from == s) selectedMoves_.push_back(m);
}

void Game::onMouseClick(float x, float y) {
    Square s = squareAt(x, y);
    if (s == NoSquare) return select(NoSquare);

    Piece p = board_.at(s);
    bool ownPiece = p != NoPiece && colorOf(p) == board_.sideToMove();

    if (selected_ == NoSquare || ownPiece) {
        // Pick up (or switch to) one of our pieces; clicking it again drops it.
        return select(ownPiece && s != selected_ ? s : NoSquare);
    }

    // A piece is selected and we clicked elsewhere: try to move there.
    Move move{.from = selected_, .to = s};
    auto legal = std::ranges::find_if(selectedMoves_,
                                      [&](const Move& m) { return m.to == s; });
    if (legal != selectedMoves_.end()) {
        move = *legal;  // carries promotion info, etc.
        // TODO: if several moves share this target (promotions), ask which piece.
    } else if (kEnforceLegalMoves) {
        return select(NoSquare);
    }

    board_.makeMove(move);
    lastMove_ = move;
    select(NoSquare);
    std::printf("%s\n", board_.toFEN().c_str());  // handy while debugging
}

void Game::onKey(const int key) {
    switch (key) {
        case GLFW_KEY_F: flipped_ = !flipped_; break;     // flip the board
        case GLFW_KEY_R:                                   // reset to the start
            board_.loadFEN(kStartFEN);
            lastMove_ = {};
            select(NoSquare);
            break;
        default: break;
    }
}

}  // namespace gui
