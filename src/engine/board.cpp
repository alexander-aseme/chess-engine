#include "engine/board.h"

#include <cctype>
#include <sstream>

namespace chess {

namespace {

constexpr std::string_view kPieceChars = "KQRBNPkqrbnp";

Piece pieceFromChar(char c) {
    auto i = kPieceChars.find(c);
    return i == std::string_view::npos ? NoPiece : Piece(i);
}

Square squareFromName(std::string_view s) {
    if (s.size() != 2 || s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return NoSquare;
    return makeSquare(s[0] - 'a', s[1] - '1');
}

std::string squareName(Square s) {
    return {char('a' + fileOf(s)), char('1' + rankOf(s))};
}

}  // namespace

Board::Board() { loadFEN(kStartFEN); }

bool Board::loadFEN(std::string_view fen) {
    std::istringstream in{std::string(fen)};
    std::string placement, side, castling, ep;
    int halfmove = 0, fullmove = 1;
    if (!(in >> placement >> side)) return false;
    in >> castling >> ep;                 // optional in some FENs
    if (!(in >> halfmove)) halfmove = 0;  // clocks are optional too
    if (!(in >> fullmove)) fullmove = 1;

    std::array<Piece, 64> sq;
    sq.fill(NoPiece);
    int rank = 7, file = 0;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8) return false;
            --rank;
            file = 0;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            file += c - '0';
            if (file > 8) return false;
        } else {
            Piece p = pieceFromChar(c);
            if (p == NoPiece || file > 7 || rank < 0) return false;
            sq[makeSquare(file++, rank)] = p;
        }
    }
    if (rank != 0 || file != 8) return false;
    if (side != "w" && side != "b") return false;

    uint8_t rights = 0;
    if (castling.empty()) castling = "-";
    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
                case 'K': rights |= WhiteKingSide; break;
                case 'Q': rights |= WhiteQueenSide; break;
                case 'k': rights |= BlackKingSide; break;
                case 'q': rights |= BlackQueenSide; break;
                default: return false;
            }
        }
    }

    Square epSq = NoSquare;
    if (!ep.empty() && ep != "-") {
        epSq = squareFromName(ep);
        if (epSq == NoSquare) return false;
    }

    squares_ = sq;
    sideToMove_ = side == "w" ? White : Black;
    castling_ = rights;
    epSquare_ = epSq;
    halfmove_ = halfmove;
    fullmove_ = fullmove;
    return true;
}

std::string Board::toFEN() const {
    std::string out;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Piece p = squares_[makeSquare(file, rank)];
            if (p == NoPiece) {
                ++empty;
                continue;
            }
            if (empty) out += char('0' + empty), empty = 0;
            out += kPieceChars[p];
        }
        if (empty) out += char('0' + empty);
        if (rank) out += '/';
    }
    out += sideToMove_ == White ? " w " : " b ";
    std::string rights;
    if (castling_ & WhiteKingSide) rights += 'K';
    if (castling_ & WhiteQueenSide) rights += 'Q';
    if (castling_ & BlackKingSide) rights += 'k';
    if (castling_ & BlackQueenSide) rights += 'q';
    out += rights.empty() ? "-" : rights;
    out += ' ';
    out += epSquare_ == NoSquare ? "-" : squareName(epSquare_);
    out += ' ' + std::to_string(halfmove_) + ' ' + std::to_string(fullmove_);
    return out;
}

void Board::makeMove(const Move& m) {
    Piece moving = squares_[m.from];
    bool capture = squares_[m.to] != NoPiece;

    squares_[m.to] = moving;
    squares_[m.from] = NoPiece;

    // TODO(movegen milestone): castling (move the rook), en passant (remove the
    // captured pawn, set epSquare_ after double pushes), promotion (use
    // m.promotion), and updating castling rights when kings/rooks move.

    halfmove_ = (typeOf(moving) == Pawn || capture) ? 0 : halfmove_ + 1;
    if (sideToMove_ == Black) ++fullmove_;
    epSquare_ = NoSquare;
    sideToMove_ = opposite(sideToMove_);
}

}  // namespace chess
