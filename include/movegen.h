#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "position.h"
#include "move.h"

class MoveGenerator {
public:
    // Generate all legal moves
    static void generate_all(const Position& pos, MoveList& moves);
    
    // Generate only captures
    static void generate_captures(const Position& pos, MoveList& moves);
    
    // Generate quiet moves (non-captures)
    static void generate_quiets(const Position& pos, MoveList& moves);
    
    // Generate evasions (when in check)
    static void generate_evasions(const Position& pos, MoveList& moves);
    
private:
    // Internal generation functions
    static void generate_pawn_moves(const Position& pos, MoveList& moves, bool captures_only, bool quiets_only);
    static void generate_knight_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets);
    static void generate_bishop_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets);
    static void generate_rook_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets);
    static void generate_queen_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets);
    static void generate_king_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets);
    
    // Check if move is legal (doesn't leave king in check)
    static bool is_legal(const Position& pos, Move m);
};

#endif // MOVEGEN_H
