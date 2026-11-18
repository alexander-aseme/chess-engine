#pragma once
#include "engine/board.h"
#include "engine/types.h"

namespace chess {

// Material values in centipawns, indexed by PieceType (King, Queen, Rook,
// Bishop, Knight, Pawn). The king has no material value: it can't be traded.
inline constexpr int kPieceValue[6] = {0, 900, 500, 330, 320, 100};

// Static evaluation: material plus piece-square tables, in centipawns.
// The king uses separate middlegame and endgame tables, blended by how much
// non-pawn material is left (the game phase).
//
// evaluate() scores from the side to move's point of view (positive = good for
// the player about to move), which is what a negamax search wants.
// evaluateWhite() scores from white's point of view, for display.
int evaluate(const Board& board);
int evaluateWhite(const Board& board);

// Game phase from 24 (all minor and major pieces on the board) down to 0 (only
// kings and pawns). Knights and bishops count 1, rooks 2, queens 4.
int gamePhase(const Board& board);

}  // namespace chess
