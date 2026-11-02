#pragma once
#include <array>
#include <string>
#include <string_view>

#include "engine/types.h"

namespace chess {

inline constexpr std::string_view kStartFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Mailbox board: one entry per square. Simple and easy to debug.
// A good later milestone is adding bitboards (one uint64_t per piece) alongside
// or instead of this array — that is where most engine speed comes from.
class Board {
public:
    Board();

    // Load a position in Forsyth–Edwards Notation. Returns false on bad input
    // and leaves the board unchanged.
    bool loadFEN(std::string_view fen);
    std::string toFEN() const;

    Piece at(Square s) const { return squares_[s]; }
    Color sideToMove() const { return sideToMove_; }
    uint8_t castlingRights() const { return castling_; }
    Square enPassantSquare() const { return epSquare_; }
    int halfmoveClock() const { return halfmove_; }
    int fullmoveNumber() const { return fullmove_; }

    // Applies a move WITHOUT checking legality, and does not yet handle
    // castling, en passant or promotion. Extending this is part of the
    // move-generation milestone (see movegen.h).
    void makeMove(const Move& m);

private:
    std::array<Piece, 64> squares_{};
    Color sideToMove_ = White;
    uint8_t castling_ = 0;
    Square epSquare_ = NoSquare;
    int halfmove_ = 0;
    int fullmove_ = 1;
};

}  // namespace chess
