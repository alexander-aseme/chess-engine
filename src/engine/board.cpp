#include "engine/board.h"

#include <cctype>
#include <cstdlib>
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

    // Rights lost when a piece leaves s or a new piece enters s (capturing).
    uint8_t rightsLostAt(Square s) {
        switch (s) {
            case A1:    return WhiteQueenSide;
            case H1:    return WhiteKingSide;
            case E1:    return WhiteKingSide | WhiteQueenSide;
            case A8:    return BlackQueenSide;
            case H8:    return BlackKingSide;
            case E8:    return BlackKingSide | BlackQueenSide;
            default:    return 0;
        }
    }

    struct RookJump { Square from, to; };

    // Where the rook goes when the king castles to kingTo.
    RookJump castlingRookJump(Square kingTo) {
        switch (kingTo) {
            case G1:    return {H1, F1};
            case C1:    return {A1, D1};
            case G8:    return {H8, F8};
            case C8:    return {A8, D8};
            default:    return {NoSquare, NoSquare};
        }
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
        PieceType type = typeOf(moving);
        bool capture = squares_[m.to] != NoPiece;

        // En passant: the captured pawn is beside the start square.
        if (type == Pawn && m.to == epSquare_) {
            squares_[makeSquare(fileOf(m.to), rankOf(m.from))] = NoPiece;
            capture = true;
        }

        // Castling: the king moves two squares, so the rook jumps over it.
        if (type == King && std::abs(fileOf(m.to) - fileOf(m.from)) == 2) {
            auto [rookFrom, rookTo] = castlingRookJump(m.to);
            squares_[rookTo] = squares_[rookFrom];
            squares_[rookFrom] = NoPiece;
        }

        squares_[m.to] = m.promotion != NoType ? makePiece(colorOf(moving), m.promotion) : moving;
        squares_[m.from] = NoPiece;

        castling_ &= ~(rightsLostAt(m.from) | rightsLostAt(m.to));
        halfmove_ = (type == Pawn || capture) ? 0 : halfmove_ + 1;
        if (sideToMove_ == Black) ++fullmove_;

        // A double push can be captured en passant on the square it skipped.
        epSquare_ = NoSquare;
        if (type == Pawn && std::abs(rankOf(m.to) - rankOf(m.from)) == 2)
            epSquare_ = makeSquare(fileOf(m.from), (rankOf(m.from) + rankOf(m.to)) / 2);

        sideToMove_ = opposite(sideToMove_);
    }

}  // namespace chess
