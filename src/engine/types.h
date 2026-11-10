#pragma once
// Core chess types shared by the engine and the GUI. Nothing under engine/
// includes gui/, so the engine builds and runs its tests without OpenGL.

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
inline constexpr bool isColor(Piece p, Color c) { return p != NoPiece && colorOf(p) == c; }

// Squares use little-endian rank-file mapping: a1 = 0, b1 = 1, ..., h8 = 63.
using Square = int;
inline constexpr Square NoSquare = -1;
inline constexpr int fileOf(const Square s) { return s & 7; }
inline constexpr int rankOf(const Square s) { return s >> 3; }
inline constexpr Square makeSquare(const int file, const int rank) { return rank * 8 + file; }

// Named squares, so A1 == makeSquare(0, 0) and H8 == makeSquare(7, 7).
enum : Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
};

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
