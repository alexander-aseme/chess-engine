#pragma once
#include <cstdint>
#include <span>

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

// True if side's king is attacked by the other colour.
bool isInCheck(const Board& board, Color side);

enum class GameState {
    Ongoing, Checkmate, Stalemate, FiftyMoveRule, ThreefoldRepetition, InsufficientMaterial
};

// True if neither side can ever checkmate: king against king, king and one
// knight or bishop against a bare king, or king and bishop against king and
// bishop with both bishops on the same square colour.
bool isInsufficientMaterial(const Board& board);

// True if `side` has nothing to mate with: a bare king, or king plus a single
// knight or bishop. Used when the other side runs out of time, which is then a
// draw instead of a loss.
bool cannotMate(const Board& board, Color side);

// Whether the game is over in this position, from the side to move's point of
// view. Threefold repetition needs the game history, so it isn't detected here.
GameState gameState(const Board& board);

// Same as above for the last position in `history` (every position of the game,
// oldest first), plus threefold repetition.
GameState gameState(std::span<const Board> history);

// True if the last position in `history` has occurred at least three times.
// Positions count as the same when the pieces, side to move, castling rights and
// any legal en passant capture all match; the clocks are ignored.
bool isThreefoldRepetition(std::span<const Board> history);

// Counts leaf nodes of the legal move tree to the given depth. The tests compare
// these counts against published reference values.
uint64_t perft(const Board& board, int depth);

}  // namespace chess
