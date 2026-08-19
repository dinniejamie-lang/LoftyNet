#include "evaluation.h"
#include <cmath>
#include <cstring>

// Piece values (centipawns)
const Score PIECE_VALUES[6] = {100, 320, 330, 500, 900, 20000};

// Piece-square tables (from white's perspective)
const Score Evaluation::PAWN_PST[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
    50,  50,  50,  50,  50,  50,  50,  50,
    10,  10,  20,  30,  30,  20,  10,  10,
    5,   5,  10,  25,  25,  10,   5,   5,
    0,   0,   0,  20,  20,   0,   0,   0,
    5,  -5, -10,   0,   0, -10,  -5,   5,
    5,  10,  10, -20, -20,  10,  10,   5,
    0,   0,   0,   0,   0,   0,   0,   0
};

const Score Evaluation::KNIGHT_PST[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50
};

const Score Evaluation::BISHOP_PST[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

const Score Evaluation::ROOK_PST[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
    5,  10,  10,  10,  10,  10,  10,   5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    0,   0,   0,   5,   5,   0,   0,   0
};

const Score Evaluation::QUEEN_PST[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -5,   0,   5,   5,   5,   5,   0,  -5,
    0,   0,   5,   5,   5,   5,   0,   0,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20
};

const Score Evaluation::KING_PST[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    20,  20,   0,   0,   0,   0,  20,  20,
    20,  30,  10,   0,   0,  10,  30,  20
};

const Score Evaluation::KING_MIDDLEGAME_PST[64] = {
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50
};

Score Evaluation::evaluate(const Position& pos) {
    Color us = pos.side_to_move();
    Color them = !us;
    
    Score score = evaluate_material(pos, us) - evaluate_material(pos, them);
    score += evaluate_pieces(pos, us) - evaluate_pieces(pos, them);
    score += evaluate_mobility(pos, us) - evaluate_mobility(pos, them);
    
    // Return score from side-to-move perspective
    return (us == WHITE) ? score : -score;
}

EvalTerms Evaluation::get_terms(const Position& pos) {
    EvalTerms terms;
    memset(&terms, 0, sizeof(terms));
    
    for (int c = 0; c < 2; ++c) {
        Color color = static_cast<Color>(c);
        terms.material[c] = evaluate_material(pos, color);
        terms.positional[c] = evaluate_pieces(pos, color);
        terms.mobility[c] = evaluate_mobility(pos, color);
        terms.king_safety[c] = evaluate_king_safety(pos, color);
        terms.pawn_structure[c] = evaluate_pawns(pos, color);
    }
    
    return terms;
}

bool Evaluation::is_draw_material(const Position& pos) {
    // King vs king
    if (bitboard_count(pos.occupied()) == 2) return true;
    
    // King + knight vs king
    if (bitboard_count(pos.occupied()) == 3) {
        if (bitboard_count(pos.occupied(WHITE)) == 2 && bitboard_count(pos.occupied(BLACK)) == 1) {
            if (pos.occupied(WHITE, KNIGHT)) return true;
        }
        if (bitboard_count(pos.occupied(BLACK)) == 2 && bitboard_count(pos.occupied(WHITE)) == 1) {
            if (pos.occupied(BLACK, KNIGHT)) return true;
        }
    }
    
    // King + bishop vs king
    if (bitboard_count(pos.occupied()) == 3) {
        if (bitboard_count(pos.occupied(WHITE)) == 2 && bitboard_count(pos.occupied(BLACK)) == 1) {
            if (pos.occupied(WHITE, BISHOP)) return true;
        }
        if (bitboard_count(pos.occupied(BLACK)) == 2 && bitboard_count(pos.occupied(WHITE)) == 1) {
            if (pos.occupied(BLACK, BISHOP)) return true;
        }
    }
    
    return false;
}

Score Evaluation::evaluate_material(const Position& pos, Color c) {
    Score material = 0;
    
    for (int pt = PAWN; pt <= KING; ++pt) {
        int count = bitboard_count(pos.occupied(c, static_cast<PieceType>(pt)));
        material += count * PIECE_VALUES[pt];
    }
    
    return material;
}

Score Evaluation::evaluate_pieces(const Position& pos, Color c) {
    Score positional = 0;
    
    // Pawns
    Bitboard pawns = pos.occupied(c, PAWN);
    while (pawns) {
        Square sq = bitboard_lsb(pawns);
        pawns = bitboard_pop_lsb(pawns);
        
        int idx = (c == WHITE) ? sq : (56 - (sq / 8) + (sq % 8));
        positional += PAWN_PST[idx];
    }
    
    // Knights
    Bitboard knights = pos.occupied(c, KNIGHT);
    while (knights) {
        Square sq = bitboard_lsb(knights);
        knights = bitboard_pop_lsb(knights);
        
        int idx = (c == WHITE) ? sq : (63 - sq);
        positional += KNIGHT_PST[idx];
    }
    
    // Bishops
    Bitboard bishops = pos.occupied(c, BISHOP);
    while (bishops) {
        Square sq = bitboard_lsb(bishops);
        bishops = bitboard_pop_lsb(bishops);
        
        int idx = (c == WHITE) ? sq : (63 - sq);
        positional += BISHOP_PST[idx];
    }
    
    // Rooks
    Bitboard rooks = pos.occupied(c, ROOK);
    while (rooks) {
        Square sq = bitboard_lsb(rooks);
        rooks = bitboard_pop_lsb(rooks);
        
        int idx = (c == WHITE) ? sq : (63 - sq);
        positional += ROOK_PST[idx];
    }
    
    // Queens
    Bitboard queens = pos.occupied(c, QUEEN);
    while (queens) {
        Square sq = bitboard_lsb(queens);
        queens = bitboard_pop_lsb(queens);
        
        int idx = (c == WHITE) ? sq : (63 - sq);
        positional += QUEEN_PST[idx];
    }
    
    // Kings
    Bitboard kings = pos.occupied(c, KING);
    while (kings) {
        Square sq = bitboard_lsb(kings);
        kings = bitboard_pop_lsb(kings);
        
        int idx = (c == WHITE) ? sq : (63 - sq);
        positional += KING_PST[idx];
    }
    
    return positional;
}

Score Evaluation::evaluate_mobility(const Position& pos, Color c) {
    Score mobility = 0;
    
    // Count attacked squares for each piece type
    Bitboard pieces = pos.occupied(c) & ~(pos.occupied(c, KING) | pos.occupied(c, PAWN));
    
    while (pieces) {
        Square sq = bitboard_lsb(pieces);
        pieces = bitboard_pop_lsb(pieces);
        
        Piece p = pos.piece_at(sq);
        switch (type_of(p)) {
            case KNIGHT:
                mobility += bitboard_count(KNIGHT_ATTACKS[sq]);
                break;
            case BISHOP:
            case ROOK:
            case QUEEN:
                // Simplified mobility count
                mobility += 2; // Base value for having sliding pieces
                break;
            default:
                break;
        }
    }
    
    return mobility;
}

Score Evaluation::evaluate_king_safety(const Position& pos, Color c) {
    // Simplified king safety - just check if king is castled
    Square ksq = SQ_NONE;
    for (int sq = 0; sq < 64; ++sq) {
        if (pos.piece_at(static_cast<Square>(sq)) == make_piece(c, KING)) {
            ksq = static_cast<Square>(sq);
            break;
        }
    }
    
    if (ksq == SQ_NONE) return 0;
    
    Score safety = 0;
    
    // Bonus for castled position
    if ((c == WHITE && (ksq == G1 || ksq == C1)) ||
        (c == BLACK && (ksq == G8 || ksq == C8))) {
        safety += 20;
    }
    
    // Penalty for exposed king
    if (c == WHITE && square_rank(ksq) > RANK_3) safety -= 10;
    if (c == BLACK && square_rank(ksq) < RANK_4) safety -= 10;
    
    return safety;
}

Score Evaluation::evaluate_pawns(const Position& pos, Color c) {
    Score pawn_score = 0;
    Bitboard pawns = pos.occupied(c, PAWN);
    
    // Check for passed pawns
    while (pawns) {
        Square sq = bitboard_lsb(pawns);
        pawns = bitboard_pop_lsb(pawns);
        pawn_score += evaluate_passed_pawn(pos, sq, c);
    }
    
    return pawn_score;
}

Score Evaluation::evaluate_passed_pawn(const Position& pos, Square s, Color c) {
    File f = square_file(s);
    Rank r = square_rank(s);
    
    Color them = !c;
    int direction = (c == WHITE) ? 1 : -1;
    
    // Check if passed (no enemy pawns on file or adjacent files ahead)
    Bitboard enemy_pawns = pos.occupied(them, PAWN);
    
    for (int rf = f - 1; rf <= f + 1; ++rf) {
        if (rf < 0 || rf > 7) continue;
        
        for (int rr = r + direction; rr >= 0 && rr < 8; rr += direction) {
            Square check_sq = make_square(static_cast<File>(rf), static_cast<Rank>(rr));
            if (enemy_pawns & (1ULL << check_sq)) {
                return 0; // Not passed
            }
        }
    }
    
    // Bonus based on rank
    int rank_bonus = (square_rank(s) - (c == WHITE ? RANK_2 : RANK_7)) * 10;
    return rank_bonus * direction;
}
