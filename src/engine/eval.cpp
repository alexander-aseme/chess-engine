#include "engine/eval.h"

#include <algorithm>

namespace chess {

namespace {

// Piece-square tables from Tomasz Michniewski's "Simplified Evaluation
// Function" (Chess Programming Wiki), in centipawns. Each table is written as
// you'd see the board from white's side: the first row is rank 8, the last row
// is rank 1. A white piece on square s reads entry s ^ 56 (flip the rank); a
// black piece reads entry s, which mirrors the table for black.

// clang-format off
constexpr int kPawnTable[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     50,  50,  50,  50,  50,  50,  50,  50,
     10,  10,  20,  30,  30,  20,  10,  10,
      5,   5,  10,  25,  25,  10,   5,   5,
      0,   0,   0,  20,  20,   0,   0,   0,
      5,  -5, -10,   0,   0, -10,  -5,   5,
      5,  10,  10, -20, -20,  10,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int kKnightTable[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

constexpr int kBishopTable[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

constexpr int kRookTable[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
      5,  10,  10,  10,  10,  10,  10,   5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
      0,   0,   0,   5,   5,   0,   0,   0,
};

constexpr int kQueenTable[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20,
};

// Middlegame: stay behind the pawns, preferably castled.
constexpr int kKingMiddlegameTable[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20,
};

// Endgame: the king becomes an attacker and belongs in the centre.
constexpr int kKingEndgameTable[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50,
};
// clang-format on

// Tables for the pieces that score the same in every phase, by PieceType.
// The king's entry is unused: it's blended separately.
constexpr const int* kTables[6] = {nullptr,      kQueenTable,  kRookTable,
                                   kBishopTable, kKnightTable, kPawnTable};

constexpr int kPhaseWeight[6] = {0, 4, 2, 1, 1, 0};
constexpr int kMaxPhase = 24;

// Index into a table written from white's side for a piece of colour c on s.
constexpr int tableIndex(Square s, Color c) { return c == White ? s ^ 56 : s; }

}  // namespace

int gamePhase(const Board& board) {
    int phase = 0;
    for (Square s = 0; s < 64; ++s)
        if (Piece p = board.at(s); p != NoPiece) phase += kPhaseWeight[typeOf(p)];
    return std::min(phase, kMaxPhase);  // extra queens from promotion don't go past 24
}

int evaluateWhite(const Board& board) {
    int score = 0;
    int kingMiddlegame = 0, kingEndgame = 0;

    for (Square s = 0; s < 64; ++s) {
        Piece p = board.at(s);
        if (p == NoPiece) continue;
        Color c = colorOf(p);
        PieceType t = typeOf(p);
        int sign = c == White ? 1 : -1;
        int i = tableIndex(s, c);

        if (t == King) {
            kingMiddlegame += sign * kKingMiddlegameTable[i];
            kingEndgame += sign * kKingEndgameTable[i];
        } else {
            score += sign * (kPieceValue[t] + kTables[t][i]);
        }
    }

    int phase = gamePhase(board);
    score += (kingMiddlegame * phase + kingEndgame * (kMaxPhase - phase)) / kMaxPhase;
    return score;
}

int evaluate(const Board& board) {
    int score = evaluateWhite(board);
    return board.sideToMove() == White ? score : -score;
}

}  // namespace chess
