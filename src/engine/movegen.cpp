#include "engine/movegen.h"
#include <span>

namespace chess {

namespace {

    struct Offset { int df, dr; };

    constexpr Offset orthogonal[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    constexpr Offset diagonal[]   = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    constexpr Offset allDirections[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                         {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    constexpr Offset knightJumps[] = {{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                       {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

    constexpr bool onBoard(int file, int rank) {
        return file >= 0 && file < 8 && rank >= 0 && rank < 8;
    }

    void addPieceMoves(const Board& board, Square from, std::span<const Offset> directions,
                    bool slides, std::vector<Move>& moves) {
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
                        moves.push_back({from, to});        // capture, then stop
                    break;                                  // blocked either way
                }
                moves.push_back({from, to});                // empty square
                if (!slides) break;                         // kings/knights: one step only
            }
        }
    }

    constexpr PieceType kPromotionTypes[] = {Queen, Rook, Bishop, Knight};

    void addPawnMoves(const Board& board, Square from, std::vector<Move>& moves) {
        Color c = colorOf(board.at(from));
        int forward   = (c == White) ? 1 : -1;   // white moves up the board, black moves down
        int startRank = (c == White) ? 1 : 6;
        int file = fileOf(from), rank = rankOf(from);

        // Single push: the square ahead must be empty.
        Square oneAhead = makeSquare(file, rank + forward);
        if (board.at(oneAhead) == NoPiece) {
            // Promotion handling
            if (rankOf(oneAhead) == 0 || rankOf(oneAhead) == 7) {
                for (PieceType type : kPromotionTypes) moves.push_back({from, oneAhead, type});
            } else {
                moves.push_back({from, oneAhead});
            }

            // Double push: only from the starting rank, and the second square must be empty too.
            Square twoAhead = makeSquare(file, rank + 2 * forward);
            if (rank == startRank && board.at(twoAhead) == NoPiece)
                moves.push_back({from, twoAhead});
        }
    }

}  // namespace

std::vector<Move> generateLegalMoves(const Board& board) {
    (void)board;
    // TODO: implement. Returning no moves for now.
    std::vector<Move> moves;
    for (int sq = 0; sq < 64; ++sq) {
        switch (typeOf(board.at(sq))) {
            case PieceType::King: addPieceMoves(board, sq, allDirections, false, moves); break;
            case PieceType::Queen: addPieceMoves(board, sq, allDirections, true, moves); break;
            case PieceType::Rook: addPieceMoves(board, sq, orthogonal, true, moves); break;
            case PieceType::Bishop: addPieceMoves(board, sq, diagonal, true, moves); break;
            case PieceType::Knight: addPieceMoves(board, sq, knightJumps, false, moves); break;
            case PieceType::Pawn: break;
            default: break;
        }
    }
    return {};
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
