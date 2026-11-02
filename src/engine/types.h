#pragma once
// Core chess types shared by the engine and the GUI.
// The engine never includes anything from gui/ — keep it that way so it can
// be tested, benchmarked and (later) run as a UCI engine with no graphics.

#include <cstdint>

namespace chess {

enum Color : uint8_t { White = 0, Black = 1 };

inline constexpr Color opposite(Color c) { return c == White ? Black : White; }

enum PieceType : uint8_t { King = 0, Queen, Rook, Bishop, Knight, Pawn, NoType };

// A piece packs colour and type: piece = color * 6 + type, NoPiece = 12.
// The order matches the sprite atlas (row = colour, column = type).
enum Piece : uint8_t {
    WKing, WQueen, WRook, WBishop, WKnight, WPawn,
    BKing, BQueen, BRook, BBishop, BKnight, BPawn,
    NoPiece
};

inline constexpr Piece makePiece(Color c, PieceType t) { return Piece(c * 6 + t); }
inline constexpr Color colorOf(Piece p) { return Color(p / 6); }
inline constexpr PieceType typeOf(Piece p) { return p == NoPiece ? NoType : PieceType(p % 6); }

// Squares use little-endian rank-file mapping: a1 = 0, b1 = 1, ..., h8 = 63.
using Square = int;
inline constexpr Square NoSquare = -1;
inline constexpr int fileOf(const Square s) { return s & 7; }
inline constexpr int rankOf(const Square s) { return s >> 3; }
inline constexpr Square makeSquare(const int file, const int rank) { return rank * 8 + file; }

// Castling rights as bit flags.
enum CastlingRight : uint8_t {
    WhiteKingSide = 1, WhiteQueenSide = 2, BlackKingSide = 4, BlackQueenSide = 8
};

struct Move {
    Square from = NoSquare;
    Square to = NoSquare;
    PieceType promotion = NoType;  // Queen/Rook/Bishop/Knight when promoting

    bool operator==(const Move&) const = default;
};

}  // namespace chess
