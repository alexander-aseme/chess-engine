#pragma once
#include <array>
#include <string>
#include <string_view>

#include "engine/types.h"

namespace chess {

inline constexpr std::string_view kStartFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Mailbox board: one Piece per square, indexed by Square, plus the rest of the
// game state a FEN records (side to move, castling rights, en passant, clocks).
class Board {
public:
    Board();

    // Load a position in Forsyth–Edwards Notation. Returns false on bad input
    // and leaves the board unchanged.
    bool loadFEN(std::string_view fen);
    std::string toFEN() const;

    Piece at(Square s) const { return squares_[s]; }
    Piece at(int file, int rank) const { return at(makeSquare(file, rank)); }
    bool isEmptyAt(Square s) const { return at(s) == NoPiece; }
    bool isEmptyAt(int file, int rank) const { return isEmptyAt(makeSquare(file, rank)); }
    Color sideToMove() const { return sideToMove_; }
    uint8_t castlingRights() const { return castling_; }
    Square enPassantSquare() const { return epSquare_; }
    int halfmoveClock() const { return halfmove_; }
    int fullmoveNumber() const { return fullmove_; }

    // Plays a pseudo-legal move: handles captures, en passant, castling (moves the
    // rook) and promotion, then updates castling rights, the en passant square,
    // both clocks and the side to move. Does not check legality.
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
