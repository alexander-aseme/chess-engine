#pragma once
#include <cstdint>

#include "engine/board.h"
#include "engine/types.h"

namespace chess {

struct MoveList {
    Move moves[256];  // no legal position has more than 218 moves
    int count = 0;

    void push(const Move& m) { moves[count++] = m; }
    int size() const { return count; }
    bool empty() const { return count == 0; }

    // Iterators for range-based for.
    Move* begin() { return moves; }
    Move* end() { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end() const { return moves + count; }
};

// Moves for the side to move that follow each piece's movement rules, including
// castling, en passant and all four promotions. Some may leave the mover's own
// king in check.
MoveList generatePseudoLegalMoves(const Board& board);

// Pseudo-legal moves that don't leave the mover's king in check. Each move is
// played on a copy of the board and kept if the king isn't attacked afterwards.
MoveList generateLegalMoves(const Board& board);

// Counts leaf nodes of the legal move tree to the given depth. The tests compare
// these counts against published reference values.
uint64_t perft(const Board& board, int depth);

}  // namespace chess
