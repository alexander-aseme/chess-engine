#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <span>

#include "engine/board.h"
#include "engine/tt.h"
#include "engine/types.h"

namespace chess {

// Scores are centipawns from the side to move's point of view, like evaluate().
// Checkmate is scored as kMateScore minus the number of plies until mate, so a
// faster mate scores higher and the engine goes for the quickest one.
inline constexpr int kMateScore = 30000;
inline constexpr int kInfinity = 32000;
inline constexpr int kMaxPly = 64;

inline constexpr bool isMateScore(int score) { return score > kMateScore - 1000 || score < -kMateScore + 1000; }

// Plies until mate for a mate score (positive: the side to move mates).
inline constexpr int mateInPlies(int score) { return score > 0 ? kMateScore - score : -kMateScore - score; }

struct SearchLimits {
    int maxDepth = kMaxPly;  // full-width depth in plies
    int64_t timeMs = 0;      // wall-clock budget for the whole search; 0 = no limit

    // For weaker play: each evaluated position gets a pseudo-random offset of up
    // to ±evalNoise centipawns, so the engine misjudges positions like a weaker
    // player. The offset depends on the position and `seed`, so it's consistent
    // within one search; vary the seed between searches.
    int evalNoise = 0;
    uint64_t seed = 0;

    // Off: lines end at the depth limit even in the middle of an exchange, so the
    // engine can walk into "I take, you retake" and lose material, as beginners do.
    bool quiescence = true;

    // Deliberate mistakes: on this percentage of moves (decided by `seed`), play a
    // random move that is between blunderMinLoss and blunderMaxLoss centipawns worse
    // than the best one, judged by a quick honest look (no noise, full capture
    // search). Moves that lose more, such as dropping the queen or allowing mate,
    // are never chosen. If no move falls in the band, the normal move is played.
    int blunderPercent = 0;
    int blunderMinLoss = 100;
    int blunderMaxLoss = 400;
};

struct SearchResult {
    Move best;           // always a legal move when one exists
    int score = 0;       // of `best`, from the side to move's point of view
    int depth = 0;       // last depth searched completely
    uint64_t nodes = 0;  // positions visited, including quiescence
    int64_t timeMs = 0;
};

// Finds the best move for the side to move in history.back().
//
// `history` is every position of the game so far, oldest first, so lines that
// repeat an earlier position are scored as draws. The search deepens one ply at
// a time (iterative deepening) until it reaches limits.maxDepth, runs out of
// time, or `stop` becomes true; it then returns the best move of the deepest
// iteration that finished.
//
// `onIteration`, if given, is called after each completed depth, e.g. to show
// progress. It runs on the searching thread.
//
// `tt` is a transposition table to use and fill. Passing the same table to every
// search in a game lets each move build on what earlier searches learned. Without
// one (or with evalNoise on), the search uses a small table of its own.
SearchResult search(std::span<const Board> history, const SearchLimits& limits,
                    const std::atomic<bool>* stop = nullptr,
                    const std::function<void(const SearchResult&)>& onIteration = {},
                    TranspositionTable* tt = nullptr);

}  // namespace chess
