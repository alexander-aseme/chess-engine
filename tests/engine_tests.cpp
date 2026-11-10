// Engine tests. Each check prints PASS or FAIL, and the program exits non-zero
// if any failed, so `ctest` reports the result.
//
//   ./build/engine_tests          FEN, en passant and check tests, perft to depth 4
//   ./build/engine_tests --deep   perft to depth 6 (several seconds)

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

    check(b.at(E1) == WKing && b.at(D8) == BQueen,
          "start position: e1 = white king, d8 = black queen");
}

static void testEnPassant() {
    // A double push offers en passant, a single push does not.
    Board b;
    b.makeMove({E2, E4});
    check(b.enPassantSquare() == E3, "double push sets en passant square");
    b.makeMove({G8, F6});  // knight
    check(b.enPassantSquare() == NoSquare, "next move clears en passant square");

    // White captures: the pawn on d5 disappears, not just the one that moved.
    b.loadFEN("4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1");
    b.makeMove({D7, D5});
    check(b.enPassantSquare() == D6, "black double push sets en passant square");
    bool offered = false;
    for (const Move& m : generateLegalMoves(b)) offered |= m.from == E5 && m.to == D6;
    check(offered, "en passant capture is generated");
    b.makeMove({E5, D6});
    check(b.toFEN() == "4k3/8/3P4/8/8/8/8/4K3 b - - 0 2", "white en passant removes captured pawn");

    // Black captures, mirrored.
    b.loadFEN("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
    b.makeMove({E2, E4});
    b.makeMove({D4, E3});
    check(b.toFEN() == "4k3/8/8/8/8/4p3/8/4K3 w - - 0 2", "black en passant removes captured pawn");

    // A normal capture elsewhere must not remove anything extra.
    b.loadFEN("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");
    b.makeMove({E4, D5});
    check(b.toFEN() == "4k3/8/8/3P4/8/8/8/4K3 b - - 0 1", "normal pawn capture unaffected");
}

static void testCheck() {
    // Own rook on e2 shields the king from the rook on e8: king has d1, f1, d2, f2
    // and the pinned rook can still slide along the e-file (e3-e8).
    Board b;
    b.loadFEN("k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    check(generateLegalMoves(b).size() == 10, "own piece blocks an attack");

    // In check from e8: the bishop can block on e2 or e6, the king can step aside.
    b.loadFEN("k3r3/8/8/8/2B5/8/8/4K3 w - - 0 1");
    check(generateLegalMoves(b).size() == 6, "blocking a check is legal");

    // In check from a1: Qxa1 captures, Qd1 blocks, Kd2/Ke2/Kf2 step away.
    // (Kf1 stays on the rook's rank, Kd1 is attacked, other queen moves ignore the check.)
    b.loadFEN("k7/8/8/8/3Q4/8/8/r3K3 w - - 0 1");
    bool canCapture = false;
    for (const Move& m : generateLegalMoves(b)) canCapture |= m.from == D4 && m.to == A1;
    check(canCapture, "capturing the checking piece is legal");
    check(generateLegalMoves(b).size() == 5, "only moves that answer the check are legal");
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
            if (got != want) break;  // a wrong count at one depth makes every deeper one wrong
        }
    }
}

int main(int argc, char** argv) {
    bool deep = argc > 1 && std::strcmp(argv[1], "--deep") == 0;
    testFEN();
    testEnPassant();
    testCheck();
    testPerft(deep ? 6 : 4);
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures,
                failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
