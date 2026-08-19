#ifndef EVALUATION_H
#define EVALUATION_H

#include "position.h"
#include <cstdint>
#include <array>

// Score type (centipawns)
using Score = int32_t;

constexpr Score SCORE_NONE = 32768;
constexpr Score SCORE_MATE = 32000;
constexpr Score SCORE_DRAW = 0;

// Material values
extern const Score PIECE_VALUES[6];

// Evaluation terms
struct EvalTerms {
    Score material[2];      // [color]
    Score positional[2];    // [color]
    Score mobility[2];      // [color]
    Score king_safety[2];   // [color]
    Score pawn_structure[2];// [color]
    Score passed_pawns[2];  // [color]
};

class Evaluation {
public:
    // Main evaluation function
    static Score evaluate(const Position& pos);
    
    // Get detailed evaluation terms
    static EvalTerms get_terms(const Position& pos);
    
    // Is position a draw by insufficient material?
    static bool is_draw_material(const Position& pos);
    
    // Material evaluation (public for NNUE hybrid evaluation)
    static Score evaluate_material(const Position& pos, Color c);
    
private:
    // Evaluation components
    static Score evaluate_pieces(const Position& pos, Color c);
    static Score evaluate_mobility(const Position& pos, Color c);
    static Score evaluate_king_safety(const Position& pos, Color c);
    static Score evaluate_pawns(const Position& pos, Color c);
    static Score evaluate_passed_pawn(const Position& pos, Square s, Color c);
    
    // Piece-square tables
    static const Score PAWN_PST[64];
    static const Score KNIGHT_PST[64];
    static const Score BISHOP_PST[64];
    static const Score ROOK_PST[64];
    static const Score QUEEN_PST[64];
    static const Score KING_PST[64];
    static const Score KING_MIDDLEGAME_PST[64];
};

#endif // EVALUATION_H
