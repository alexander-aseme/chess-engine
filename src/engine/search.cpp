#include "engine/search.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "engine/eval.h"
#include "engine/movegen.h"

namespace chess {

namespace {

using Clock = std::chrono::steady_clock;

using Bound = TranspositionTable::Bound;

// SplitMix64's finaliser: scrambles a 64-bit value so every input bit affects
// every output bit. Turns a key or seed into well-spread pseudo-random bits.
uint64_t mix64(uint64_t h) {
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
    return h ^ (h >> 31);
}

bool isCapture(const Board& b, const Move& m) {
    return !b.isEmptyAt(m.to) || (typeOf(b.at(m.from)) == Pawn && m.to == b.enPassantSquare());
}

// Mate scores count plies from the root ("mate in 5 from here"), but a table entry
// can be reused at another distance from the root. Store them as plies from the
// position itself, and convert back when reading.
int scoreToTable(int score, int ply) {
    if (score > kMateScore - 1000) return score + ply;
    if (score < -kMateScore + 1000) return score - ply;
    return score;
}

int scoreFromTable(int score, int ply) {
    if (score > kMateScore - 1000) return score - ply;
    if (score < -kMateScore + 1000) return score + ply;
    return score;
}

// Alpha-beta search over the game's position stack. One Searcher runs one search.
class Searcher {
public:
    Searcher(std::span<const Board> history, const SearchLimits& limits, const std::atomic<bool>* stop,
             TranspositionTable* tt)
        : limits_(limits), stop_(stop), start_(Clock::now()) {
        // Every position stays in stack_ so repetitions can be found; reserving up
        // front keeps references to stack_.back() valid while children are pushed.
        stack_.reserve(history.size() + kMaxPly + 2);
        stack_.assign(history.begin(), history.end());

        // A shared table carries knowledge from one move to the next. With noise on,
        // scores depend on this search's seed, so they mustn't mix with other
        // searches: use a private table then (and when none was given).
        if (tt && limits.evalNoise == 0) {
            tt_ = tt;
        } else {
            ownTable_ = std::make_unique<TranspositionTable>(4);
            tt_ = ownTable_.get();
        }
    }

    SearchResult run(const std::function<void(const SearchResult&)>& onIteration);

private:
    int negamax(int depth, int ply, int alpha, int beta);
    int quiesce(int ply, int alpha, int beta);
    Move pickBlunder(const Move& normal);

    bool isRepetition() const;
    int evaluateNoisy(const Board& b) const;
    void countNode();
    void orderMoves(MoveList& moves, const Board& b, int ply, const Move& first) const;
    int64_t elapsedMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count();
    }

    // Plays m on top of the stack. Returns false (and undoes it) if it leaves the
    // mover's king in check, which is how pseudo-legal moves are filtered.
    bool push(const Move& m);
    void pop() { stack_.pop_back(); }

    SearchLimits limits_;
    // Set while pickBlunder() scores moves honestly: no noise, full capture search,
    // no table (whose entries hold the weakened scores), no time limit.
    bool honest_ = false;
    const std::atomic<bool>* stop_;
    Clock::time_point start_;
    bool stopped_ = false;
    uint64_t nodes_ = 0;

    TranspositionTable* tt_ = nullptr;
    std::unique_ptr<TranspositionTable> ownTable_;

    std::vector<Board> stack_;
    Move rootBest_;               // best root move so far in this iteration
    Move killers_[kMaxPly][2]{};  // quiet moves that caused a cutoff at each ply
};

bool Searcher::push(const Move& m) {
    Color mover = stack_.back().sideToMove();
    stack_.push_back(stack_.back());
    stack_.back().makeMove(m);
    if (isInCheck(stack_.back(), mover)) {
        stack_.pop_back();
        return false;
    }
    return true;
}

// Checks the clock and the stop flag every 1024 positions; reading the time on
// every node would cost more than the check is worth.
void Searcher::countNode() {
    if (++nodes_ % 1024 != 0 || honest_) return;  // the honest scoring is tiny; let it finish
    if (stop_ && stop_->load(std::memory_order_relaxed)) stopped_ = true;
    if (limits_.timeMs > 0 && elapsedMs() >= limits_.timeMs) stopped_ = true;
}

// Has the current position occurred before, since the last capture or pawn move?
// Even one earlier occurrence counts: if repeating is good for one side it can
// repeat again, so the line is a draw either way.
bool Searcher::isRepetition() const {
    const Board& cur = stack_.back();
    int n = int(stack_.size()) - 1;
    for (int i = n - 2; i >= 0 && n - i <= cur.halfmoveClock(); i -= 2)
        if (stack_[i].key() == cur.key()) return true;  // same Zobrist key: same position
    return false;
}

// evaluate(), plus the weakening offset from limits_.evalNoise: the position's
// Zobrist key mixed with the seed, mapped to [-evalNoise, +evalNoise].
int Searcher::evaluateNoisy(const Board& b) const {
    int score = evaluate(b);
    if (limits_.evalNoise <= 0 || honest_) return score;
    uint64_t h = mix64(b.key() ^ limits_.seed);  // the key is unique per position; mix in the seed
    return score + int(h % uint64_t(2 * limits_.evalNoise + 1)) - limits_.evalNoise;
}

// Best moves first, so alpha-beta can cut the rest short: the previous
// iteration's best move, then captures (most valuable victim, then cheapest
// attacker), promotions, and finally quiet moves that caused cutoffs at this ply.
void Searcher::orderMoves(MoveList& moves, const Board& b, int ply, const Move& first) const {
    std::pair<int, Move> scored[256];
    for (int i = 0; i < moves.size(); ++i) {
        const Move& m = moves.moves[i];
        int score = 0;
        if (m == first) {
            score = 1'000'000;
        } else if (isCapture(b, m)) {
            PieceType victim = b.isEmptyAt(m.to) ? Pawn : typeOf(b.at(m.to));  // empty target: en passant
            score = 100'000 + 10 * kPieceValue[victim] - kPieceValue[typeOf(b.at(m.from))];
        } else if (m.promotion != NoType) {
            score = 90'000 + kPieceValue[m.promotion];
        } else if (m == killers_[ply][0]) {
            score = 80'000;
        } else if (m == killers_[ply][1]) {
            score = 79'000;
        }
        scored[i] = {score, m};
    }
    std::stable_sort(scored, scored + moves.size(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (int i = 0; i < moves.size(); ++i) moves.moves[i] = scored[i].second;
}

int Searcher::negamax(int depth, int ply, int alpha, int beta) {
    countNode();
    if (stopped_) return 0;
    const Board& b = stack_.back();

    if (ply > 0) {
        if (isRepetition() || isInsufficientMaterial(b)) return 0;
        // A mate found closer to the root already beats anything this deep.
        alpha = std::max(alpha, -kMateScore + ply);
        beta = std::min(beta, kMateScore - ply - 1);
        if (alpha >= beta) return alpha;
    }

    bool inCheck = isInCheck(b, b.sideToMove());
    if (inCheck) ++depth;  // look one ply further when in check, so mating attacks are seen through
    if (depth <= 0 || ply >= kMaxPly - 1)
        return limits_.quiescence || honest_ ? quiesce(ply, alpha, beta) : evaluateNoisy(b);

    // Searched this position before, at least as deeply? Then the stored score may
    // settle it without searching: an exact score always does, a bound does when it
    // falls outside the window. Either way its best move is tried first. (Not at the
    // root, which must always come up with a move of its own.)
    Move ttMove;
    if (const auto* e = honest_ ? nullptr : tt_->probe(b.key())) {
        ttMove = e->best;
        if (ply > 0 && e->depth >= depth) {
            int s = scoreFromTable(e->score, ply);
            if (e->bound == Bound::Exact) return s;
            if (e->bound == Bound::Lower && s >= beta) return s;
            if (e->bound == Bound::Upper && s <= alpha) return s;
        }
    }

    MoveList moves = generatePseudoLegalMoves(b);
    orderMoves(moves, b, ply, ply == 0 ? rootBest_ : ttMove);

    int alphaAtStart = alpha;
    int best = -kInfinity;
    Move bestMove;
    int legal = 0;
    for (const Move& m : moves) {
        bool quiet = !isCapture(b, m) && m.promotion == NoType;
        if (!push(m)) continue;
        ++legal;
        int score = -negamax(depth - 1, ply + 1, -beta, -alpha);
        pop();
        if (stopped_) return 0;

        if (score > best) {
            best = score;
            bestMove = m;
            if (score > alpha) {
                alpha = score;
                if (ply == 0) rootBest_ = m;
            }
        }
        if (alpha >= beta) {
            if (quiet && !(m == killers_[ply][0])) {
                killers_[ply][1] = killers_[ply][0];
                killers_[ply][0] = m;
            }
            break;
        }
    }

    if (legal == 0) return inCheck ? -kMateScore + ply : 0;  // checkmate or stalemate
    if (b.halfmoveClock() >= 100) return 0;                 // after the mate check: mate wins

    Bound bound = best <= alphaAtStart ? Bound::Upper : best >= beta ? Bound::Lower : Bound::Exact;
    if (!honest_) tt_->store(b.key(), depth, scoreToTable(best, ply), bound, bestMove);
    return best;
}

// At the end of the main search, keep playing captures (and promotions) until the
// position is quiet, so a line doesn't stop right after "I take your queen" and
// miss the recapture. The side to move may also "stand pat" and take the static
// evaluation instead of capturing. In check there's no standing pat: every
// evasion is searched.
int Searcher::quiesce(int ply, int alpha, int beta) {
    countNode();
    if (stopped_) return 0;
    const Board& b = stack_.back();
    bool inCheck = isInCheck(b, b.sideToMove());

    int best = -kInfinity;
    if (!inCheck) {
        best = evaluateNoisy(b);
        if (best >= beta || ply >= kMaxPly - 1) return best;
        alpha = std::max(alpha, best);
    } else if (ply >= kMaxPly - 1) {
        return evaluateNoisy(b);
    }

    MoveList moves = generatePseudoLegalMoves(b);
    orderMoves(moves, b, ply, Move{});
    int legal = 0;
    for (const Move& m : moves) {
        if (!inCheck && !isCapture(b, m) && m.promotion == NoType) continue;
        if (!push(m)) continue;
        ++legal;
        int score = -quiesce(ply + 1, -beta, -alpha);
        pop();
        if (stopped_) return 0;

        if (score > best) {
            best = score;
            alpha = std::max(alpha, score);
        }
        if (alpha >= beta) break;
    }
    if (inCheck && legal == 0) return -kMateScore + ply;  // checkmated
    return best;
}

SearchResult Searcher::run(const std::function<void(const SearchResult&)>& onIteration) {
    SearchResult result;
    MoveList rootMoves = generateLegalMoves(stack_.back());
    if (rootMoves.empty()) return result;  // game over: nothing to play
    result.best = rootMoves.moves[0];      // something legal, even if time runs out at once

    int maxDepth = std::clamp(limits_.maxDepth, 1, kMaxPly - 1);
    for (int depth = 1; depth <= maxDepth; ++depth) {
        int score = negamax(depth, 0, -kInfinity, kInfinity);

        if (stopped_) {
            // An unfinished iteration can't be trusted, except at depth 1 where it's
            // still better than the unsorted first move.
            if (depth == 1 && rootBest_.from != NoSquare) result.best = rootBest_;
            break;
        }

        result.best = rootBest_;
        result.score = score;
        result.depth = depth;
        result.nodes = nodes_;
        result.timeMs = elapsedMs();
        if (onIteration) onIteration(result);

        if (rootMoves.size() == 1) break;  // only one move: no need to look further
        if (isMateScore(score) && std::abs(mateInPlies(score)) <= depth) break;  // forced mate found
        // The next iteration takes several times longer than this one; don't start
        // it if it can't finish.
        if (limits_.timeMs > 0 && elapsedMs() > limits_.timeMs / 2) break;
    }

    bool cancelled = stop_ && stop_->load(std::memory_order_relaxed);
    if (limits_.blunderPercent > 0 && rootMoves.size() > 1 && !cancelled) result.best = pickBlunder(result.best);

    result.nodes = nodes_;
    result.timeMs = elapsedMs();
    return result;
}

// On blunderPercent% of moves, swap the chosen move for a random one that's
// clearly worse but not absurd. Each legal move gets an honest score from a short
// look ahead (the move, the best reply, then captures until quiet), and the
// candidates are the moves losing between blunderMinLoss and blunderMaxLoss
// against the best of them.
Move Searcher::pickBlunder(const Move& normal) {
    uint64_t dice = mix64(limits_.seed ^ 0xB1D0'B1D0'B1D0'B1D0ull);  // independent of the noise
    if (int(dice % 100) >= limits_.blunderPercent) return normal;

    honest_ = true;
    bool wasStopped = stopped_;
    stopped_ = false;
    std::vector<std::pair<Move, int>> scored;
    int best = -kInfinity;
    for (const Move& m : generateLegalMoves(stack_.back())) {
        stack_.push_back(stack_.back());
        stack_.back().makeMove(m);
        int score = -negamax(1, 1, -kInfinity, kInfinity);
        stack_.pop_back();
        scored.emplace_back(m, score);
        best = std::max(best, score);
    }
    honest_ = false;
    stopped_ = wasStopped;

    std::vector<Move> candidates;
    for (const auto& [m, score] : scored) {
        int loss = best - score;
        if (loss >= limits_.blunderMinLoss && loss <= limits_.blunderMaxLoss) candidates.push_back(m);
    }
    if (candidates.empty()) return normal;
    return candidates[(dice >> 32) % candidates.size()];
}

}  // namespace

SearchResult search(std::span<const Board> history, const SearchLimits& limits, const std::atomic<bool>* stop,
                    const std::function<void(const SearchResult&)>& onIteration, TranspositionTable* tt) {
    if (history.empty()) return {};
    Searcher searcher(history, limits, stop, tt);
    return searcher.run(onIteration);
}

}  // namespace chess
