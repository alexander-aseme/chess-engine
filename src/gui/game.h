#pragma once
#include <vector>

#include "engine/board.h"
#include "engine/movegen.h"
#include "gui/renderer.h"

namespace gui {

// The playable chess view: owns the board state and turns mouse/keyboard input
// into moves. All chess rules live in the engine; this class only asks it.
class Game {
public:
    bool init();  // load textures; call after the GL context exists
    void shutdown();

    void draw(Renderer& r, float windowW, float windowH);
    void onMouseClick(float x, float y);
    void onKey(int key);

private:
    // Board geometry, recomputed every frame so the board resizes with the window.
    Rect boardRect_{};
    float squareSize_ = 0;

    chess::Board board_;
    chess::Square selected_ = chess::NoSquare;
    std::vector<chess::Move> selectedMoves_;  // legal moves from selected_
    chess::Move lastMove_{};
    bool flipped_ = false;  // view from black's side
    Texture pieces_;

    chess::Square squareAt(float x, float y) const;
    Rect squareRect(chess::Square s) const;
    void select(chess::Square s);
};

}  // namespace gui
