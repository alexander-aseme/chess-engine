#pragma once
#include <cstdint>

namespace chess {

// Zobrist hashing: one fixed random 64-bit number for every piece on every
// square, plus numbers for the side to move, the castling rights and the en
// passant file. A position's key is the XOR of the numbers for everything in
// it (see Board::key()), so a move updates the key by XOR-ing out what changed
// and XOR-ing in what's new. Equal positions get equal keys whatever move
// order led to them; different positions almost never do (64 bits).
//
// The numbers are generated at compile time from a fixed seed, so they are the
// same on every run and every platform.
struct ZobristKeys {
    uint64_t piece[12][64];  // [Piece][Square]
    uint64_t blackToMove;
    uint64_t castling[16];   // one per combination of the four castling-right bits
    uint64_t enPassant[8];   // by file
};

namespace detail {

// SplitMix64: a small, well-mixed pseudo-random generator, usable in constexpr.
constexpr uint64_t splitMix64(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

constexpr ZobristKeys makeZobristKeys() {
    ZobristKeys k{};
    uint64_t state = 0x5EED'C0FF'EE15'600Dull;
    for (auto& piece : k.piece)
        for (auto& square : piece) square = splitMix64(state);
    k.blackToMove = splitMix64(state);
    for (auto& c : k.castling) c = splitMix64(state);
    for (auto& e : k.enPassant) e = splitMix64(state);
    return k;
}

}  // namespace detail

inline constexpr ZobristKeys kZobrist = detail::makeZobristKeys();

}  // namespace chess
