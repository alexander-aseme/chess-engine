// Engine tests. No framework needed: each check prints PASS/FAIL and the
// program exits non-zero if anything failed, so `ctest` picks it up.
//
//   ./build/engine_tests          FEN tests + quick perft (depth <= 4)
//   ./build/engine_tests --deep   also runs the slow, deeper perft counts

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include "engine/board.h"
#include "engine/movegen.h"

using namespace chess;

static int failures = 0;

static void check(bool ok, const std::string& name) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", name.c_str());
    if (!ok) ++failures;
}

static void testFEN() {
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
    };
    for (const char* fen : fens) {
        Board b;
        bool loaded = b.loadFEN(fen);
        check(loaded && b.toFEN() == fen, std::string("FEN round-trip: ") + fen);
    }

    Board b;
    check(!b.loadFEN("not a fen"), "rejects garbage FEN");
    check(!b.loadFEN("8/8/8/8/8/8/8/9 w - - 0 1"), "rejects rank with >8 files");
    check(b.toFEN() == kStartFEN, "failed load leaves board unchanged");

    check(b.at(makeSquare(4, 0)) == WKing && b.at(makeSquare(3, 7)) == BQueen,
          "start position: e1 = white king, d8 = black queen");
}

// Reference counts from the Chess Programming Wiki "Perft Results" page.
struct PerftCase {
    const char* name;
    const char* fen;
    uint64_t expected[6];  // depth 1..6, 0 = not listed
};

static const PerftCase kPerft[] = {
    {"start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {20, 400, 8902, 197281, 4865609, 119060324}},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {48, 2039, 97862, 4085603, 193690690, 0}},
    {"position3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {14, 191, 2812, 43238, 674624, 11030083}},
    {"position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {6, 264, 9467, 422333, 15833292, 0}},
    {"position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {44, 1486, 62379, 2103487, 89941194, 0}},
};

static void testPerft(int maxDepth) {
    Board start;
    if (generateLegalMoves(start).empty()) {
        std::printf("SKIP  perft: generateLegalMoves() not implemented yet\n");
        return;
    }
    for (const auto& c : kPerft) {
        Board b;
        b.loadFEN(c.fen);
        for (int d = 1; d <= maxDepth; ++d) {
            uint64_t want = c.expected[d - 1];
            if (!want) continue;
            auto t0 = std::chrono::steady_clock::now();
            uint64_t got = perft(b, d);
            double ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0).count();
            check(got == want, std::string("perft ") + c.name + " depth " +
                                   std::to_string(d) + ": got " + std::to_string(got) +
                                   ", want " + std::to_string(want) + " (" +
                                   std::to_string(int(ms)) + " ms)");
            if (got != want) break;  // deeper depths will fail too
        }
    }
}

int main(int argc, char** argv) {
    bool deep = argc > 1 && std::strcmp(argv[1], "--deep") == 0;
    testFEN();
    testPerft(deep ? 6 : 4);
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures,
                failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
