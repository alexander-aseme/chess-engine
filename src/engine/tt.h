#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/types.h"

namespace chess {

// A transposition table: what the search learned about positions it has already
// searched, indexed by Zobrist key, so a position reached again (by another move
// order, or in the next iteration of iterative deepening) isn't searched from
// scratch. Each entry remembers how deeply the position was searched, the score,
// whether that score is exact or only a bound, and the best move found, which is
// tried first next time.
//
// Fixed size: when two positions map to the same slot, the newer or deeper result
// replaces the older one.
class TranspositionTable {
public:
    // Alpha-beta often only proves a bound: "at least this" when a move was good
    // enough to cut off the search (Lower), "at most this" when no move reached
    // alpha (Upper).
    enum class Bound : uint8_t { None, Exact, Lower, Upper };

    struct Entry {
        uint64_t key = 0;
        Move best;
        int16_t score = 0;
        int8_t depth = 0;
        Bound bound = Bound::None;
    };

    explicit TranspositionTable(std::size_t megabytes = 16) { resize(megabytes); }

    void resize(std::size_t megabytes) {
        // A power-of-two number of entries, so a key maps to a slot with a bit mask.
        std::size_t count = 1;
        while (count * 2 * sizeof(Entry) <= megabytes * 1024 * 1024) count *= 2;
        entries_.assign(count, Entry{});
        mask_ = count - 1;
    }

    void clear() { entries_.assign(entries_.size(), Entry{}); }

    // The entry for this exact position, or nullptr if the slot holds another one.
    const Entry* probe(uint64_t key) const {
        const Entry& e = entries_[key & mask_];
        return e.bound != Bound::None && e.key == key ? &e : nullptr;
    }

    void store(uint64_t key, int depth, int score, Bound bound, const Move& best) {
        Entry& e = entries_[key & mask_];
        // Keep a deeper result for the same position over a shallower one; anything
        // else (a different position, or as deep or deeper) replaces it.
        if (e.key == key && e.bound != Bound::None && depth < e.depth) return;
        e = {key, best, int16_t(score), int8_t(depth), bound};
    }

private:
    std::vector<Entry> entries_;
    std::size_t mask_ = 0;
};

}  // namespace chess
