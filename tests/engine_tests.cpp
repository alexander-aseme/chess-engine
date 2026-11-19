// Engine tests. Each check prints PASS or FAIL, and the program exits non-zero
// if any failed, so `ctest` reports the result.
//
//   ./build/engine_tests          FEN, en passant and check tests, perft to depth 4
//   ./build/engine_tests --deep   perft to depth 6 (several seconds)

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/board.h"
#include "engine/clock.h"
#include "engine/eval.h"
#include "engine/movegen.h"
#include "engine/notation.h"
#include "engine/search.h"

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

static void testGameState() {
    Board b;
    check(gameState(b) == GameState::Ongoing && !isInCheck(b, White), "start position is ongoing");

    // Fool's mate: Qh4 mates white.
    b.loadFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    check(isInCheck(b, White) && gameState(b) == GameState::Checkmate, "fool's mate is checkmate");

    // Black king on h8 has no moves and isn't in check.
    b.loadFEN("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    check(gameState(b) == GameState::Stalemate, "stalemate detected");

    b.loadFEN("8/8/8/8/8/8/k7/4K2R w K - 100 80");
    check(gameState(b) == GameState::FiftyMoveRule, "fifty-move rule at halfmove 100");
}

static bool insufficient(const char* fen) {
    Board b;
    b.loadFEN(fen);
    return isInsufficientMaterial(b);
}

static void testInsufficientMaterial() {
    check(insufficient("8/8/4k3/8/8/4K3/8/8 w - - 0 1"), "K v K is a draw");
    check(insufficient("8/8/4k3/8/8/4K3/5N2/8 w - - 0 1"), "K+N v K is a draw");
    check(insufficient("8/8/4k3/8/8/4K3/5B2/8 w - - 0 1"), "K+B v K is a draw");
    // f2 and d6 are both dark squares; c6 is light.
    check(insufficient("8/8/3bk3/8/8/4K3/5B2/8 w - - 0 1"), "K+B v K+B, same-colour bishops, is a draw");
    check(!insufficient("8/8/2b1k3/8/8/4K3/5B2/8 w - - 0 1"), "opposite-colour bishops can still mate");
    check(!insufficient("8/8/4k3/8/8/4K3/4NN2/8 w - - 0 1"), "two knights are not an automatic draw");
    check(!insufficient("8/8/4k3/8/8/4K3/4P3/8 w - - 0 1"), "a pawn can still win");
    check(!insufficient(kStartFEN.data()), "start position has mating material");

    Board b;
    b.loadFEN("8/8/4k3/8/8/4K3/8/8 w - - 0 1");
    check(gameState(b) == GameState::InsufficientMaterial, "gameState reports insufficient material");

    b.loadFEN("8/8/4k3/8/8/4K3/5N2/7q w - - 0 1");
    check(cannotMate(b, White) && !cannotMate(b, Black), "cannotMate: K+N can't, K+Q can");
}

static void testClock() {
    ChessClock c;
    c.reset(0, 0);
    check(!c.enabled(), "untimed clock is disabled");

    c.reset(60'000, 2'000);  // 1 minute + 2 seconds
    check(c.enabled() && !c.running() && c.remaining(White, 5'000) == 60'000, "clock starts stopped");

    c.start(White, 1'000);
    check(c.remaining(White, 11'000) == 50'000 && c.remaining(Black, 11'000) == 60'000,
          "only the running side's time goes down");

    c.press(11'000);  // white moves after 10 s
    check(c.runningSide() == Black && c.remaining(White, 20'000) == 52'000,
          "press adds the increment and switches sides");

    c.stop(21'000);
    check(!c.running() && c.remaining(Black, 99'000) == 50'000, "stop keeps the time used");

    c.start(Black, 100'000);
    check(!c.flagged(149'999) && c.flagged(150'000) && c.remaining(Black, 200'000) == 0,
          "flag falls at zero and time never goes negative");
}

// Plays `moves` from `fen` and returns every position, the starting one first.
static std::vector<Board> playLine(const char* fen, std::initializer_list<Move> moves) {
    std::vector<Board> history(1);
    history[0].loadFEN(fen);
    for (const Move& m : moves) {
        history.push_back(history.back());
        history.back().makeMove(m);
    }
    return history;
}

static void testRepetition() {
    auto shuffle = playLine(kStartFEN.data(), {{G1, F3}, {G8, F6}, {F3, G1}, {F6, G8},
                                               {G1, F3}, {G8, F6}, {F3, G1}, {F6, G8}});
    check(!isThreefoldRepetition(std::span(shuffle).first(5)), "a position seen twice is not a draw");
    check(isThreefoldRepetition(shuffle) && gameState(shuffle) == GameState::ThreefoldRepetition,
          "knights shuffling back home three times is a draw");

    // 1.e4 leaves an en passant square that no black pawn can use, so the position
    // after 1.e4 matches the later ones where the square isn't set.
    auto afterPush = playLine(kStartFEN.data(), {{E2, E4}, {G8, F6}, {G1, F3}, {F6, G8}, {F3, G1},
                                                 {G8, F6}, {G1, F3}, {F6, G8}, {F3, G1}});
    check(isThreefoldRepetition(afterPush), "an unusable en passant square doesn't block repetition");

    // Moving the rooks away and back loses castling rights, so the first position
    // (with rights) differs from the later ones that look the same.
    auto rooks = playLine("r3k3/8/8/8/8/8/8/R3K3 w Qq - 0 1",
                          {{A1, A2}, {A8, A7}, {A2, A1}, {A7, A8}, {A1, A2}, {A8, A7}, {A2, A1}, {A7, A8},
                           {A1, A2}, {A8, A7}, {A2, A1}, {A7, A8}});
    check(!isThreefoldRepetition(std::span(rooks).first(9)), "castling rights are part of the position");
    check(isThreefoldRepetition(rooks), "same position without castling rights, three times");
}

// The same position with colours swapped and the board flipped top to bottom:
// ranks reversed, piece case swapped, side to move and castling rights swapped,
// en passant rank mirrored. A fair evaluation scores both the same for the side
// to move.
static std::string mirrorFEN(const std::string& fen) {
    auto swapCase = [](char ch) {
        return std::isupper((unsigned char)ch) ? char(std::tolower((unsigned char)ch))
                                               : char(std::toupper((unsigned char)ch));
    };
    std::vector<std::string> fields;
    for (size_t start = 0, end; start <= fen.size(); start = end + 1) {
        end = fen.find(' ', start);
        if (end == std::string::npos) end = fen.size();
        fields.push_back(fen.substr(start, end - start));
    }

    std::vector<std::string> ranks;
    for (size_t start = 0, end; start <= fields[0].size(); start = end + 1) {
        end = fields[0].find('/', start);
        if (end == std::string::npos) end = fields[0].size();
        ranks.push_back(fields[0].substr(start, end - start));
    }
    std::string placement;
    for (int i = int(ranks.size()) - 1; i >= 0; --i) {
        for (char ch : ranks[i]) placement += swapCase(ch);
        if (i > 0) placement += '/';
    }

    std::string castling;
    for (char want : std::string("KQkq"))
        if (fields[2].find(swapCase(want)) != std::string::npos) castling += want;
    if (castling.empty()) castling = "-";

    std::string ep = fields[3];
    if (ep != "-") ep[1] = ep[1] == '3' ? '6' : '3';

    return placement + (fields[1] == "w" ? " b " : " w ") + castling + " " + ep + " " +
           fields[4] + " " + fields[5];
}

static int evalWhite(const char* fen) {
    Board b;
    b.loadFEN(fen);
    return evaluateWhite(b);
}

static void testEval() {
    Board b;
    check(evaluate(b) == 0, "start position evaluates to 0");
    check(gamePhase(b) == 24, "start position is full middlegame phase");

    const char* fens[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "8/8/4k3/8/2K5/8/3P4/8 b - - 0 1",
    };
    for (const char* fen : fens) {
        Board a, m;
        a.loadFEN(fen);
        bool loaded = m.loadFEN(mirrorFEN(fen));
        check(loaded && evaluate(a) == evaluate(m), std::string("colour-mirror symmetric: ") + fen);
    }

    // Same position, other side to move: the score flips sign.
    Board w, bl;
    w.loadFEN(fens[0]);
    std::string blackToMove = fens[0];
    blackToMove.replace(blackToMove.find(" w "), 3, " b ");
    bl.loadFEN(blackToMove);
    check(evaluate(w) == -evaluate(bl) && evaluateWhite(w) == evaluateWhite(bl),
          "side to move flips the sign of evaluate()");

    // Material: black is missing its queen.
    check(evalWhite("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") > 800,
          "an extra queen is worth about 900");

    // Piece-square tables: a centralised knight beats one in the corner.
    check(evalWhite("4k3/8/8/8/4N3/8/8/4K3 w - - 0 1") >
              evalWhite("4k3/8/8/8/8/8/8/N3K3 w - - 0 1"),
          "knight on e4 scores higher than on a1");
    check(evalWhite("4k3/8/8/8/8/8/3PP3/4K3 w - - 0 1") <
              evalWhite("4k3/8/8/8/3PP3/8/8/4K3 w - - 0 1"),
          "advanced centre pawns score higher than unmoved ones");

    // King: tucked away in the middlegame, centralised in the endgame.
    check(evalWhite("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1RK1 w kq - 0 1") >
              evalWhite("rnbqkbnr/pppppppp/8/8/4K3/8/PPPPPPPP/RNBQ1R2 w kq - 0 1"),
          "middlegame king prefers safety to the centre");
    check(evalWhite("4k3/8/8/8/4K3/8/8/8 w - - 0 1") >
              evalWhite("4k3/8/8/8/8/8/8/K7 w - - 0 1"),
          "endgame king prefers the centre to the corner");
}

static std::string san(const char* fen, Move m) {
    Board b;
    b.loadFEN(fen);
    return toSAN(b, m);
}

static void testSAN() {
    check(squareName(E4) == "e4" && squareName(A1) == "a1" && squareName(H8) == "h8",
          "square names");
    check(san(kStartFEN.data(), {E2, E4}) == "e4", "SAN pawn push");
    check(san(kStartFEN.data(), {G1, F3}) == "Nf3", "SAN knight move");
    check(san("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", {E4, D5}) == "exd5",
          "SAN pawn capture");
    check(san("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3", {E5, F6}) == "exf6",
          "SAN en passant");
    check(san("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {E1, G1}) == "O-O", "SAN kingside castle");
    check(san("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", {E8, C8}) == "O-O-O",
          "SAN queenside castle");
    check(san("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1", {B1, D2}) == "Nbd2",
          "SAN disambiguates by file");
    check(san("4k3/8/8/8/R7/8/8/R3K3 w - - 0 1", {A1, A3}) == "R1a3",
          "SAN disambiguates by rank");
    check(san("4k3/8/8/8/8/8/8/Q1Q1KQ2 w - - 0 1", {C1, B2}) == "Qcb2" &&
              san("4k3/8/8/8/8/8/8/Q1Q1KQ2 w - - 0 1", {C1, D2}) == "Qd2",
          "SAN disambiguates only when another piece reaches the same square");
    check(san("7k/8/8/8/2Q1Q3/8/2Q5/4K3 w - - 0 1", {C4, D3}) == "Qc4d3",
          "SAN uses the full square when file and rank are both shared");
    check(san("7k/4P3/8/8/8/8/8/4K3 w - - 0 1", {E7, E8, Queen}) == "e8=Q+",
          "SAN promotion with check");
    check(san("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2", {D8, H4}) == "Qh4#",
          "SAN checkmate");
}

// Visits every position up to `depth` plies from b and counts those whose
// incrementally updated key differs from the key computed from scratch.
static int keyMismatches(const Board& b, int depth) {
    int bad = b.key() != b.computeKey();
    if (depth == 0) return bad;
    for (const Move& m : generateLegalMoves(b)) {
        Board next = b;
        next.makeMove(m);
        bad += keyMismatches(next, depth - 1);
    }
    return bad;
}

static uint64_t keyOf(const char* fen) {
    Board b;
    b.loadFEN(fen);
    return b.key();
}

static void testZobrist() {
    // The perft positions between them exercise castling, en passant and every promotion.
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    };
    int mismatches = 0;
    for (const char* fen : fens) {
        Board b;
        b.loadFEN(fen);
        mismatches += keyMismatches(b, 3);
    }
    check(mismatches == 0, "incremental key equals key from scratch in every position 3 plies deep (" +
                               std::to_string(mismatches) + " mismatches)");

    auto a = playLine(kStartFEN.data(), {{E2, E4}, {G8, F6}, {B1, C3}});
    auto b = playLine(kStartFEN.data(), {{B1, C3}, {G8, F6}, {E2, E4}});
    check(a.back().key() == b.back().key(), "transposed move orders give the same key");

    check(keyOf("4k3/8/8/8/8/8/8/4K2R w K - 0 1") != keyOf("4k3/8/8/8/8/8/8/4K2R b K - 0 1"),
          "side to move changes the key");
    check(keyOf("4k3/8/8/8/8/8/8/4K2R w K - 0 1") != keyOf("4k3/8/8/8/8/8/8/4K2R w - - 0 1"),
          "castling rights change the key");
    check(keyOf("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2") != keyOf("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 2"),
          "a usable en passant square changes the key");
    check(keyOf("4k3/8/8/3p4/8/8/8/4K3 w - d6 0 2") == keyOf("4k3/8/8/3p4/8/8/8/4K3 w - - 0 2"),
          "an unusable en passant square doesn't");
    check(keyOf("4k3/8/8/8/8/8/8/4K2R w K - 0 1") == keyOf("4k3/8/8/8/8/8/8/4K2R w K - 37 60"),
          "the clocks aren't part of the key");
}

static SearchResult searchFEN(const char* fen, int depth, int64_t timeMs = 0) {
    Board b;
    b.loadFEN(fen);
    return search(std::span(&b, 1), {.maxDepth = depth, .timeMs = timeMs});
}

static bool isLegal(const Board& b, const Move& m) {
    for (const Move& legal : generateLegalMoves(b))
        if (legal == m) return true;
    return false;
}

static void testSearch() {
    // Back-rank mate: Ra8#.
    SearchResult r = searchFEN("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", 3);
    check(r.best == Move{A1, A8} && r.score == kMateScore - 1, "search finds mate in 1 (back rank)");

    // Scholar's mate: Qxf7#.
    r = searchFEN("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", 3);
    check(r.best == Move{H5, F7} && r.score == kMateScore - 1, "search finds mate in 1 (scholar's mate)");

    // Morphy's problem, mate in 2: 1.Ra6! bxa6 2.b7#, or 1...Bxb6/Bc7 etc. 2.Rxa7#.
    r = searchFEN("kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", 5);
    check(r.best == Move{A1, A6} && r.score == kMateScore - 3, "search finds mate in 2 (Ra6!)");

    // Being mated: black's king is boxed in (Bc4 covers g8, Kh6 covers g7/h7), so
    // whichever pawn move it plays, Rb8# follows. Mate against the side to move in
    // 2 plies scores -(kMateScore - 2). (With a single legal move the search stops
    // after depth 1, so this position gives black two pawn moves to choose from.)
    r = searchFEN("7k/p7/7K/8/2B5/8/8/1R6 b - - 0 1", 3);
    check(r.score == -(kMateScore - 2), "search scores a forced mate against it");

    // A free queen is taken.
    r = searchFEN("4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1", 3);
    check(r.best == Move{D2, D5}, "search takes a free queen");

    // Qxd5 wins a pawn but loses the queen to cxd5: quiescence sees the recapture.
    r = searchFEN("4k3/8/2p5/3p4/8/8/3Q4/4K3 w - - 0 1", 2);
    check(!(r.best == Move{D2, D5}), "search doesn't trade its queen for a defended pawn");

    // Winning with K+Q v K, but many queen moves stalemate: the search must not pick one.
    Board stale;
    stale.loadFEN("7k/8/5Q2/8/8/8/8/K7 w - - 0 1");
    r = search(std::span(&stale, 1), {.maxDepth = 4});
    Board after = stale;
    after.makeMove(r.best);
    check(gameState(after) != GameState::Stalemate && r.score > 500, "search avoids stalemating when winning");

    // With a game history (here the current position has occurred before), the
    // search still returns a legal move.
    auto shuffle = playLine("7k/8/8/8/8/8/1Q6/K7 b - - 0 1", {{H8, G8}, {B2, B3}, {G8, H8}, {B3, B2}});
    r = search(shuffle, {.maxDepth = 3});
    check(isLegal(shuffle.back(), r.best), "search with game history returns a legal move");

    // Every result is legal; the time limit is respected; a stop request still yields a move.
    Board start;
    r = search(std::span(&start, 1), {.maxDepth = 4});
    check(isLegal(start, r.best) && r.depth == 4, "search returns a legal move at depth 4 from the start");

    auto t0 = std::chrono::steady_clock::now();
    r = search(std::span(&start, 1), {.maxDepth = kMaxPly, .timeMs = 300});
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    check(isLegal(start, r.best) && ms < 600, "search stops near its time limit (" + std::to_string(ms) +
                                                  " ms for 300, depth " + std::to_string(r.depth) + ")");

    std::atomic<bool> stopNow{true};
    r = search(std::span(&start, 1), {.maxDepth = 10}, &stopNow);
    check(isLegal(start, r.best), "search stopped at once still returns a legal move");

    // Transposition table: entries come back for their own key only, and a deeper
    // result isn't overwritten by a shallower one for the same position.
    TranspositionTable tt(1);
    tt.store(12345, 5, 77, TranspositionTable::Bound::Exact, Move{E2, E4});
    const auto* e = tt.probe(12345);
    tt.store(12345, 2, -10, TranspositionTable::Bound::Upper, Move{D2, D4});
    check(e && tt.probe(12345)->score == 77 && tt.probe(12345)->best == (Move{E2, E4}) && !tt.probe(54321),
          "transposition table stores, finds, and keeps the deeper entry");

    // A shared table reused across searches (as the game does move after move) must
    // still give exact mate scores: they're stored relative to each position.
    tt.clear();
    Board morphy;
    morphy.loadFEN("kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1");
    SearchResult first = search(std::span(&morphy, 1), {.maxDepth = 5}, nullptr, {}, &tt);
    SearchResult again = search(std::span(&morphy, 1), {.maxDepth = 5}, nullptr, {}, &tt);
    check(first.best == Move{A1, A6} && again.best == Move{A1, A6} && again.score == kMateScore - 3 &&
              again.nodes < first.nodes,
          "a reused table keeps mate scores exact and saves work (" + std::to_string(first.nodes) + " then " +
              std::to_string(again.nodes) + " nodes)");

    // Weakening: without the capture search, a 1-ply search grabs the defended pawn
    // and loses its queen to cxd5, a typical beginner's mistake.
    Board greedy;
    greedy.loadFEN("4k3/8/2p5/3p4/8/8/3Q4/4K3 w - - 0 1");
    r = search(std::span(&greedy, 1), {.maxDepth = 1, .quiescence = false});
    check(r.best == Move{D2, D5}, "with the capture search off, it falls for a defended pawn");

    // Deliberate mistakes. The pawn on d5 attacks the knight: every knight move saves
    // it, every king move loses it (about 3 pawns worse, inside the 1-4 pawn band).
    Board attacked;
    attacked.loadFEN("4k3/8/8/3p4/4N3/8/8/4K3 w - - 0 1");
    r = search(std::span(&attacked, 1), {.maxDepth = 3});
    SearchResult blunder = search(std::span(&attacked, 1), {.maxDepth = 3, .seed = 7, .blunderPercent = 100});
    check(r.best.from == E4 && blunder.best.from == E1,
          "a deliberate mistake picks a clearly worse move (" + toSAN(attacked, blunder.best) + " instead of " +
              toSAN(attacked, r.best) + ")");

    // ...but never one that's far worse. With mate in one available, every other
    // move is vastly worse than the band allows, so the mate is still played.
    Board backRank;
    backRank.loadFEN("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1");
    blunder = search(std::span(&backRank, 1), {.maxDepth = 3, .seed = 7, .blunderPercent = 100});
    check(blunder.best == Move{A1, A8}, "a deliberate mistake never throws away a mate in one");

    Board mated;
    mated.loadFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    r = search(std::span(&mated, 1), {.maxDepth = 3});
    check(r.best.from == NoSquare, "search returns no move when the game is over");
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
    testGameState();
    testRepetition();
    testInsufficientMaterial();
    testClock();
    testEval();
    testSAN();
    testZobrist();
    testSearch();
    testPerft(deep ? 6 : 4);
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures,
                failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
