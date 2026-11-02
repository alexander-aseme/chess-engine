#include "engine/movegen.h"
#include <span>

namespace chess {

namespace {

    struct Offset { int df, dr; };

    constexpr Offset orthogonal[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    constexpr Offset diagonal[]   = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    constexpr Offset allDirections[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    constexpr Offset knightJumps[] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

    constexpr PieceType promotionTypes[] = {Queen, Rook, Bishop, Knight};

    constexpr bool onBoard(int file, int rank) {
        return file >= 0 && file < 8 && rank >= 0 && rank < 8;
    }

    void doAddMoves(const Board& board, Square from, std::span<const Offset> directions, bool slides, MoveList& moves) {
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

    void addStepMoves(const Board& board, Square from, std::span<const Offset> offsets, MoveList& moves) {
        doAddMoves(board, from, offsets, false, moves);
    }

    void addSlidingMoves(const Board& board, Square from, std::span<const Offset> directions, MoveList& moves) {
        doAddMoves(board, from, directions, true, moves);
    }

    void addMoveWithPromotions(Square from, Square to, MoveList& moves) {
        if (rankOf(to) == 0 || rankOf(to) == 7) {
            for (PieceType type : promotionTypes) moves.push({from, to, type});
        } else {
            moves.push({from, to});
        }
    }

    void addPawnMoves(const Board& board, Square from, MoveList& moves) {
        Color c = board.sideToMove();
        int forward   = (c == White) ? 1 : -1;   // white moves up the board, black moves down
        int startRank = (c == White) ? 1 : 6;
        int file = fileOf(from), rank = rankOf(from);

        // Single push: the square ahead must be empty.
        Square oneAhead = makeSquare(file, rank + forward);
        if (board.at(oneAhead) == NoPiece) {
            addMoveWithPromotions(from, oneAhead, moves);

            // Double push: only from the starting rank, and the second square must be empty too.
            Square twoAhead = makeSquare(file, rank + 2 * forward);
            if (rank == startRank && board.at(twoAhead) == NoPiece)
                moves.push({from, twoAhead});
        }

        // Captures (including en passant): one square forward and one to each side.
        for (int df : {-1, 1}) {
            int toFile = file + df;
            if (!onBoard(toFile, rank)) continue;              // off the side of the board

            Square to = makeSquare(toFile, rank + forward);
            Piece target = board.at(to);

            if (target != NoPiece && colorOf(target) != c) {
                addMoveWithPromotions(from, to, moves);
            } else if (to == board.enPassantSquare()) {
                moves.push({from, to});                     // en passant never promotes
            }
        }

    }

}  // namespace

MoveList generateLegalMoves(const Board& board) {
    MoveList moves;
    for (int sq = 0; sq < 64; ++sq) {
        Piece p = board.at(sq);
        if (p == NoPiece || colorOf(p) != board.sideToMove()) continue;

        switch (typeOf(p)) {
            case King:   addStepMoves(board, sq, allDirections, moves);    break;
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

uint64_t perft(const Board& board, int depth) {
    if (depth == 0) return 1;
    uint64_t nodes = 0;
    for (const Move& m : generateLegalMoves(board)) {
        Board next = board;  // copy-make: simple and fast enough to start with
        next.makeMove(m);
        nodes += perft(next, depth - 1);
    }
    return nodes;
}

}  // namespace chess
