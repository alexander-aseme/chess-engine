#include "engine/movegen.h"
#include <span>

namespace chess {

    namespace {

        struct Offset { int df, dr; };  // file and rank step

        constexpr Offset orthogonal[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        constexpr Offset diagonal[]   = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        constexpr Offset allDirections[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        constexpr Offset knightJumps[] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

        constexpr PieceType promotionTypes[] = {Queen, Rook, Bishop, Knight};

        constexpr bool onBoard(int file, int rank) {
            return file >= 0 && file < 8 && rank >= 0 && rank < 8;
        }

        // Per-colour constants, so the move code needs no White/Black branches.
        constexpr int pawnForward(Color c)        { return c == White ? 1 : -1; }  // white moves up the board
        constexpr int pawnStartRank(Color c)      { return c == White ? 1 : 6; }
        constexpr uint8_t kingSideRight(Color c)  { return c == White ? WhiteKingSide : BlackKingSide; }
        constexpr uint8_t queenSideRight(Color c) { return c == White ? WhiteQueenSide : BlackQueenSide; }

        // True if any piece of colour `by` attacks s. Works outward from s: looks
        // for a pawn, knight or king on the squares one could attack from, then
        // walks each line to the first piece to find rooks, bishops and queens.
        bool isSquareAttacked(const Board& board, Square s, Color by) {
            int file = fileOf(s), rank = rankOf(s);

            // Is there a `by` piece of this type at (f, r)?
            auto hasPiece = [&](int f, int r, PieceType t) {
                return onBoard(f, r) && board.at(f, r) == makePiece(by, t);
            };

            // Pawns attack diagonally forward, so look one rank behind s from their side.
            int behind = -pawnForward(by);
            if (hasPiece(file - 1, rank + behind, Pawn) || hasPiece(file + 1, rank + behind, Pawn))
                return true;

            for (auto [df, dr] : knightJumps)
                if (hasPiece(file + df, rank + dr, Knight)) return true;
            for (auto [df, dr] : allDirections)
                if (hasPiece(file + df, rank + dr, King)) return true;

            // Sliders: walk each line to the first piece and check if it moves that way.
            auto lineAttacked = [&](std::span<const Offset> directions, PieceType slider) {
                for (auto [df, dr] : directions) {
                    for (int f = file + df, r = rank + dr; onBoard(f, r); f += df, r += dr) {
                        Piece p = board.at(f, r);
                        if (p == NoPiece) continue;
                        if (p == makePiece(by, slider) || p == makePiece(by, Queen)) return true;
                        break;  // any other piece blocks the rest of this line
                    }
                }
                return false;
            };
            return lineAttacked(orthogonal, Rook) || lineAttacked(diagonal, Bishop);
        }

        // True if side's king is attacked by the other colour.
        bool isInCheck(const Board& board, Color side) {
            for (Square s = 0; s < 64; ++s)
                if (board.at(s) == makePiece(side, King))
                    return isSquareAttacked(board, s, opposite(side));
            return false;
        }

        // Steps from `from` in each direction, adding a move for every empty square
        // and stopping at the first piece (a capture if it's an enemy). With
        // slides == false only the first step is taken (king, knight).
        void addRayMoves(const Board& board, Square from, std::span<const Offset> directions, bool slides, MoveList& moves) {
            for (auto [df, dr] : directions) {
                int file = fileOf(from), rank = rankOf(from);
                while (true) {
                    file += df;
                    rank += dr;
                    if (!onBoard(file, rank)) break;            // fell off the board

                    Square to = makeSquare(file, rank);
                    Piece target = board.at(to);
                    if (target != NoPiece) {
                        if (colorOf(target) != board.sideToMove())
                            moves.push({from, to});        // capture, then stop
                        break;                                  // blocked either way
                    }
                    moves.push({from, to});                // empty square
                    if (!slides) break;                         // kings/knights: one step only
                }
            }
        }

        void addCastlingMoves(const Board& board, Square from, MoveList& moves) {
            Color c = board.sideToMove();
            Color them = opposite(c);

            auto safe = [&](Square s) { return !isSquareAttacked(board, s, them); };

            if (isSquareAttacked(board, from, them)) return;  // no castling out of check
            if (from != (c == White ? E1 : E8)) return;       // king must be on its home square

            // Squares between the king and each rook: f, g on the king side; d, c, b on the queen side.
            // The king crosses f, g or d, c, so those must also be unattacked; b only needs to be empty.
            bool kingsideOpen  = board.isEmptyAt(from + 1) && board.isEmptyAt(from + 2);
            bool queensideOpen = board.isEmptyAt(from - 1) && board.isEmptyAt(from - 2) && board.isEmptyAt(from - 3);

            if (kingsideOpen && (board.castlingRights() & kingSideRight(c)) && safe(from + 1) && safe(from + 2))
                moves.push({from, from + 2});
            if (queensideOpen && (board.castlingRights() & queenSideRight(c)) && safe(from - 1) && safe(from - 2))
                moves.push({from, from - 2});
        }

        void addMoveWithPromotions(Square from, Square to, MoveList& moves) {
            if (rankOf(to) == 0 || rankOf(to) == 7) {
                for (PieceType type : promotionTypes) moves.push({from, to, type});
            } else {
                moves.push({from, to});
            }
        }

        void addStepMoves(const Board& board, Square from, std::span<const Offset> offsets, MoveList& moves) {
            addRayMoves(board, from, offsets, false, moves);
        }

        void addSlidingMoves(const Board& board, Square from, std::span<const Offset> directions, MoveList& moves) {
            addRayMoves(board, from, directions, true, moves);
        }

        void addKingMoves(const Board& board, Square from, MoveList& moves) {
            addStepMoves(board, from, allDirections, moves);
            addCastlingMoves(board, from, moves);
        }

        void addPawnMoves(const Board& board, Square from, MoveList& moves) {
            Color c = board.sideToMove();
            int forward = pawnForward(c);
            int file = fileOf(from), rank = rankOf(from);

            // Single push: the square ahead must be empty.
            Square oneAhead = makeSquare(file, rank + forward);
            if (board.isEmptyAt(oneAhead)) {
                addMoveWithPromotions(from, oneAhead, moves);

                // Double push: only from the starting rank, and the second square must be empty too.
                Square twoAhead = makeSquare(file, rank + 2 * forward);
                if (rank == pawnStartRank(c) && board.isEmptyAt(twoAhead))
                    moves.push({from, twoAhead});
            }

            // Captures (including en passant): one square forward and one to each side.
            for (int df : {-1, 1}) {
                int toFile = file + df;
                if (!onBoard(toFile, rank)) continue;              // off the side of the board

                Square to = makeSquare(toFile, rank + forward);
                Piece target = board.at(to);

                if (isColor(target, opposite(c))) {
                    addMoveWithPromotions(from, to, moves);
                } else if (to == board.enPassantSquare()) {
                    moves.push({from, to});                     // en passant never promotes
                }
            }

        }

    }  // namespace

    MoveList generatePseudoLegalMoves(const Board& board) {
        MoveList moves;
        for (Square sq = 0; sq < 64; ++sq) {
            Piece p = board.at(sq);
            if (!isColor(p,board.sideToMove())) continue;

            switch (typeOf(p)) {
                case King:   addKingMoves(board, sq, moves);    break;
                case Queen:  addSlidingMoves(board, sq, allDirections, moves); break;
                case Rook:   addSlidingMoves(board, sq, orthogonal, moves);    break;
                case Bishop: addSlidingMoves(board, sq, diagonal, moves);      break;
                case Knight: addStepMoves(board, sq, knightJumps, moves);      break;
                case Pawn:   addPawnMoves(board, sq, moves);                   break;
                default:     break;
            }
        }
        return moves;
    }

    MoveList generateLegalMoves(const Board& board) {
        MoveList pseudoLegal = generatePseudoLegalMoves(board);
        MoveList legal;
        for (const Move& move : pseudoLegal) {
            Board next = board;
            next.makeMove(move);
            if (!isInCheck(next, board.sideToMove())) legal.push(move);
        }
        return legal;
    }

    uint64_t perft(const Board& board, int depth) {
        if (depth == 0) return 1;
        uint64_t nodes = 0;
        for (const Move& m : generateLegalMoves(board)) {
            Board next = board;  // copy-make: the board is small, so no unmakeMove is needed
            next.makeMove(m);
            nodes += perft(next, depth - 1);
        }
        return nodes;
    }

}  // namespace chess
