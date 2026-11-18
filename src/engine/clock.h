#pragma once
#include <algorithm>
#include <cstdint>

#include "engine/types.h"

namespace chess {

// A chess clock: time left for each side, an increment added after every move,
// and at most one side running. It never reads the system time itself; every
// call takes "now" in milliseconds, so the GUI supplies a real clock and the
// tests supply made-up times.
class ChessClock {
public:
    // Both sides get `initialMs`; `incrementMs` is added after each move.
    // initialMs == 0 means an untimed game: the clock is disabled.
    void reset(int64_t initialMs, int64_t incrementMs) {
        remaining_[White] = remaining_[Black] = initialMs;
        increment_ = incrementMs;
        enabled_ = initialMs > 0;
        running_ = false;
    }

    bool enabled() const { return enabled_; }
    bool running() const { return running_; }
    Color runningSide() const { return side_; }

    // Starts (or resumes) counting down for `side`.
    void start(Color side, int64_t now) {
        side_ = side;
        since_ = now;
        running_ = true;
    }

    // Stops counting; the time used so far is kept.
    void stop(int64_t now) {
        if (!running_) return;
        remaining_[side_] = remaining(side_, now);
        running_ = false;
    }

    // The running side has moved: it gets its increment and the other side starts.
    void press(int64_t now) {
        if (!running_) return;
        Color mover = side_;
        stop(now);
        remaining_[mover] += increment_;
        start(opposite(mover), now);
    }

    // Time left for `side` at `now`, never below zero.
    int64_t remaining(Color side, int64_t now) const {
        int64_t left = remaining_[side];
        if (running_ && side == side_) left -= now - since_;
        return std::max<int64_t>(0, left);
    }

    // True if the running side has used up all its time.
    bool flagged(int64_t now) const { return running_ && remaining(side_, now) == 0; }

private:
    int64_t remaining_[2] = {0, 0};
    int64_t increment_ = 0;
    bool enabled_ = false;
    bool running_ = false;
    Color side_ = White;
    int64_t since_ = 0;
};

}  // namespace chess
