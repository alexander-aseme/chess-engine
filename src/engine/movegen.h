#pragma once
#include <cstdint>
#include <vector>

#include "engine/board.h"

namespace chess {

// ---------------------------------------------------------------------------
// YOUR NEXT MILESTONE: move generation.
//
// Suggested order:
//   1. Pseudo-legal moves for each piece type (sliders, knights, king, pawns).
//   2. Pawn specials: double push, en passant, promotion (4 moves per push).
//   3. Castling (rights, empty squares, king not passing through check).
//   4. Legality: drop moves that leave your own king in check.
//   5. Finish Board::makeMove (castling, en passant, promotion) and add
//      unmakeMove (or copy-make) so search can undo moves.
//   6. Run the perft tests in tests/engine_tests.cpp until every one passes.
//
// The GUI already calls generateLegalMoves() to show legal-move dots and,
// once you flip kEnforceLegalMoves in src/gui/game.cpp, to reject illegal moves.
// ---------------------------------------------------------------------------

std::vector<Move> generateLegalMoves(const Board& board);

// Counts leaf nodes of the legal move tree to the given depth. The single most
// useful correctness test for a move generator.
uint64_t perft(const Board& board, int depth);

}  // namespace chess
