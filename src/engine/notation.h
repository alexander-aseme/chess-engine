#pragma once
#include <string>

#include "engine/board.h"
#include "engine/types.h"

namespace chess {

// "e4", "a1", ... ; "-" for NoSquare.
std::string squareName(Square s);

// Standard Algebraic Notation for a legal move m played from `before`:
// "e4", "Nf3", "exd5", "Nbd2", "R1a3", "e8=Q", "O-O", with "+" for check and
// "#" for checkmate. m must be legal in `before`.
std::string toSAN(const Board& before, const Move& m);

}  // namespace chess
