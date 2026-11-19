#include "engine/board.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "engine/zobrist.h"

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
        key_ = computeKey();
        return true;
    }

    // A pawn of the side to move stands beside the pawn that just double-pushed,
    // so it could capture en passant. (Whether that capture would be legal isn't
    // checked; a pinned pawn still counts, which only makes a few positions hash
    // differently that are really the same.)
    bool Board::enPassantCapturable() const {
        if (epSquare_ == NoSquare) return false;
        int pawnRank = sideToMove_ == White ? 4 : 3;  // rank the capturing pawn stands on
        for (int df : {-1, 1}) {
            int f = fileOf(epSquare_) + df;
            if (f >= 0 && f < 8 && at(f, pawnRank) == makePiece(sideToMove_, Pawn)) return true;
        }
        return false;
    }

    uint64_t Board::stateKey() const {
        uint64_t k = kZobrist.castling[castling_];
        if (sideToMove_ == Black) k ^= kZobrist.blackToMove;
        if (enPassantCapturable()) k ^= kZobrist.enPassant[fileOf(epSquare_)];
        return k;
    }

    uint64_t Board::computeKey() const {
        uint64_t k = stateKey();
        for (Square s = 0; s < 64; ++s)
            if (squares_[s] != NoPiece) k ^= kZobrist.piece[squares_[s]][s];
        return k;
    }

    void Board::put(Square s, Piece p) {
        squares_[s] = p;
        key_ ^= kZobrist.piece[p][s];
    }

    void Board::clear(Square s) {
        key_ ^= kZobrist.piece[squares_[s]][s];
        squares_[s] = NoPiece;
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

        // The key: take out the old side to move, castling rights and en passant
        // here; put the new ones back at the end. Pieces are updated as they move.
        key_ ^= stateKey();

        // En passant: the captured pawn is beside the start square.
        if (type == Pawn && m.to == epSquare_) {
            clear(makeSquare(fileOf(m.to), rankOf(m.from)));
            capture = true;
        }

        // Castling: the king moves two squares, so the rook jumps over it.
        if (type == King && std::abs(fileOf(m.to) - fileOf(m.from)) == 2) {
            auto [rookFrom, rookTo] = castlingRookJump(m.to);
            Piece rook = squares_[rookFrom];
            clear(rookFrom);
            put(rookTo, rook);
        }

        if (squares_[m.to] != NoPiece) clear(m.to);  // normal capture
        clear(m.from);
        put(m.to, m.promotion != NoType ? makePiece(colorOf(moving), m.promotion) : moving);

        castling_ &= ~(rightsLostAt(m.from) | rightsLostAt(m.to));
        halfmove_ = (type == Pawn || capture) ? 0 : halfmove_ + 1;
        if (sideToMove_ == Black) ++fullmove_;

        // A double push can be captured en passant on the square it skipped.
        epSquare_ = NoSquare;
        if (type == Pawn && std::abs(rankOf(m.to) - rankOf(m.from)) == 2)
            epSquare_ = makeSquare(fileOf(m.from), (rankOf(m.from) + rankOf(m.to)) / 2);

        sideToMove_ = opposite(sideToMove_);
        key_ ^= stateKey();
    }

}  // namespace chess
