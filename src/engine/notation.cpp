#include "engine/notation.h"

#include <cstdlib>

#include "engine/movegen.h"

namespace chess {

namespace {

// SAN letter for each PieceType (King, Queen, Rook, Bishop, Knight, Pawn).
constexpr char kPieceLetter[6] = {'K', 'Q', 'R', 'B', 'N', '\0'};

char fileChar(Square s) { return char('a' + fileOf(s)); }
char rankChar(Square s) { return char('1' + rankOf(s)); }

}  // namespace

std::string squareName(Square s) {
    if (s == NoSquare) return "-";
    return {fileChar(s), rankChar(s)};
}

std::string toSAN(const Board& before, const Move& m) {
    PieceType type = typeOf(before.at(m.from));
    std::string san;

    if (type == King && std::abs(fileOf(m.to) - fileOf(m.from)) == 2) {
        san = fileOf(m.to) == 6 ? "O-O" : "O-O-O";
    } else {
        bool capture = !before.isEmptyAt(m.to) ||
                       (type == Pawn && m.to == before.enPassantSquare());

        if (type == Pawn) {
            if (capture) san += fileChar(m.from);  // exd5: pawn captures name their file
        } else {
            san += kPieceLetter[type];

            // Another piece of the same kind that can also reach m.to makes the move
            // ambiguous. Prefer the file to tell them apart, then the rank, then both.
            bool ambiguous = false, sameFile = false, sameRank = false;
            for (const Move& other : generateLegalMoves(before)) {
                if (other.to != m.to || other.from == m.from) continue;
                if (before.at(other.from) != before.at(m.from)) continue;
                ambiguous = true;
                sameFile |= fileOf(other.from) == fileOf(m.from);
                sameRank |= rankOf(other.from) == rankOf(m.from);
            }
            if (ambiguous) {
                if (!sameFile) san += fileChar(m.from);
                else if (!sameRank) san += rankChar(m.from);
                else san += squareName(m.from);
            }
        }

        if (capture) san += 'x';
        san += squareName(m.to);
        if (m.promotion != NoType) {
            san += '=';
            san += kPieceLetter[m.promotion];
        }
    }

    Board after = before;
    after.makeMove(m);
    if (isInCheck(after, after.sideToMove()))
        san += generateLegalMoves(after).empty() ? '#' : '+';
    return san;
}

}  // namespace chess
