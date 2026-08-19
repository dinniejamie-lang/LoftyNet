#include "movegen.h"
#include <cstring>

// Helper to check if a pseudo-legal move is actually legal
static bool is_legal_quick(Position& pos, Move m) {
    Square from = m.from_sq();
    Square to = m.to_sq();
    
    // Make the move temporarily
    State st;
    pos.save_state(st);
    pos.make_move(m);
    
    // Find our king
    Color us = st.side_to_move;
    Square ksq = SQ_NONE;
    for (int sq = 0; sq < 64; ++sq) {
        if (pos.piece_at(static_cast<Square>(sq)) == make_piece(us, KING)) {
            ksq = static_cast<Square>(sq);
            break;
        }
    }
    
    // Check if king is attacked
    bool in_check = (ksq != SQ_NONE) && pos.is_attacked(ksq, !us);
    
    // Unmake the move
    pos.unmake_move(m, st);
    
    return !in_check;
}

void MoveGenerator::generate_all(const Position& pos, MoveList& moves) {
    moves.clear();
    Color us = pos.side_to_move();
    Color them = !us;
    Bitboard our_pieces = pos.occupied(us);
    Bitboard their_pieces = pos.occupied(them);
    Bitboard targets = their_pieces;
    Bitboard empty = ~pos.occupied();
    
    bool in_check = pos.in_check();
    
    if (in_check) {
        generate_evasions(pos, moves);
        return;
    }
    
    // Pawn moves
    generate_pawn_moves(pos, moves, false, false);
    
    // Knight moves
    Bitboard knights = pos.occupied(us, KNIGHT);
    while (knights) {
        Square from = bitboard_lsb(knights);
        knights = bitboard_pop_lsb(knights);
        generate_knight_moves(pos, moves, from, their_pieces | empty);
    }
    
    // Bishop moves
    Bitboard bishops = pos.occupied(us, BISHOP);
    while (bishops) {
        Square from = bitboard_lsb(bishops);
        bishops = bitboard_pop_lsb(bishops);
        generate_bishop_moves(pos, moves, from, their_pieces | empty);
    }
    
    // Rook moves
    Bitboard rooks = pos.occupied(us, ROOK);
    while (rooks) {
        Square from = bitboard_lsb(rooks);
        rooks = bitboard_pop_lsb(rooks);
        generate_rook_moves(pos, moves, from, their_pieces | empty);
    }
    
    // Queen moves
    Bitboard queens = pos.occupied(us, QUEEN);
    while (queens) {
        Square from = bitboard_lsb(queens);
        queens = bitboard_pop_lsb(queens);
        generate_queen_moves(pos, moves, from, their_pieces | empty);
    }
    
    // King moves
    Square ksq = SQ_NONE;
    for (int sq = 0; sq < 64; ++sq) {
        if (pos.piece_at(static_cast<Square>(sq)) == make_piece(us, KING)) {
            ksq = static_cast<Square>(sq);
            break;
        }
    }
    if (ksq != SQ_NONE) {
        generate_king_moves(pos, moves, ksq, their_pieces | empty);
        
        // Castling
        uint8_t castling = pos.castling_rights();
        if (us == WHITE) {
            if ((castling & WHITE_OO) && 
                pos.piece_at(F1) == NO_PIECE && pos.piece_at(G1) == NO_PIECE &&
                !pos.is_attacked(E1, them) && !pos.is_attacked(F1, them) && !pos.is_attacked(G1, them)) {
                moves.add(Move(E1, G1, MOVE_CASTLING));
            }
            if ((castling & WHITE_OOO) &&
                pos.piece_at(D1) == NO_PIECE && pos.piece_at(C1) == NO_PIECE && pos.piece_at(B1) == NO_PIECE &&
                !pos.is_attacked(E1, them) && !pos.is_attacked(D1, them) && !pos.is_attacked(C1, them)) {
                moves.add(Move(E1, C1, MOVE_CASTLING));
            }
        } else {
            if ((castling & BLACK_OO) &&
                pos.piece_at(F8) == NO_PIECE && pos.piece_at(G8) == NO_PIECE &&
                !pos.is_attacked(E8, them) && !pos.is_attacked(F8, them) && !pos.is_attacked(G8, them)) {
                moves.add(Move(E8, G8, MOVE_CASTLING));
            }
            if ((castling & BLACK_OOO) &&
                pos.piece_at(D8) == NO_PIECE && pos.piece_at(C8) == NO_PIECE && pos.piece_at(B8) == NO_PIECE &&
                !pos.is_attacked(E8, them) && !pos.is_attacked(D8, them) && !pos.is_attacked(C8, them)) {
                moves.add(Move(E8, C8, MOVE_CASTLING));
            }
        }
    }
    
    // Filter illegal moves
    MoveList legal_moves;
    for (int i = 0; i < moves.size(); ++i) {
        if (is_legal_quick(const_cast<Position&>(pos), moves[i])) {
            legal_moves.add(moves[i]);
        }
    }
    moves = legal_moves;
}

void MoveGenerator::generate_captures(const Position& pos, MoveList& moves) {
    moves.clear();
    Color us = pos.side_to_move();
    Color them = !us;
    Bitboard their_pieces = pos.occupied(them);
    
    bool in_check = pos.in_check();
    if (in_check) {
        generate_evasions(pos, moves);
        // Filter to only captures
        MoveList captures;
        for (int i = 0; i < moves.size(); ++i) {
            if (pos.piece_at(moves[i].to_sq()) != NO_PIECE || moves[i].type() == MOVE_EN_PASSANT) {
                captures.add(moves[i]);
            }
        }
        moves = captures;
        return;
    }
    
    generate_pawn_moves(pos, moves, true, false);
    
    Bitboard attackers = pos.occupied(us) & ~(pos.occupied(us, PAWN) | pos.occupied(us, KING));
    while (attackers) {
        Square from = bitboard_lsb(attackers);
        attackers = bitboard_pop_lsb(attackers);
        Piece p = pos.piece_at(from);
        
        switch (type_of(p)) {
            case KNIGHT:
                generate_knight_moves(pos, moves, from, their_pieces);
                break;
            case BISHOP:
                generate_bishop_moves(pos, moves, from, their_pieces);
                break;
            case ROOK:
                generate_rook_moves(pos, moves, from, their_pieces);
                break;
            case QUEEN:
                generate_queen_moves(pos, moves, from, their_pieces);
                break;
            default:
                break;
        }
    }
    
    // Filter illegal
    MoveList legal;
    for (int i = 0; i < moves.size(); ++i) {
        if (is_legal_quick(const_cast<Position&>(pos), moves[i])) {
            legal.add(moves[i]);
        }
    }
    moves = legal;
}

void MoveGenerator::generate_quiets(const Position& pos, MoveList& moves) {
    moves.clear();
    Color us = pos.side_to_move();
    Bitboard empty = ~pos.occupied();
    
    bool in_check = pos.in_check();
    if (in_check) {
        generate_evasions(pos, moves);
        // Filter to only non-captures
        MoveList quiets;
        for (int i = 0; i < moves.size(); ++i) {
            if (pos.piece_at(moves[i].to_sq()) == NO_PIECE && moves[i].type() != MOVE_EN_PASSANT) {
                quiets.add(moves[i]);
            }
        }
        moves = quiets;
        return;
    }
    
    generate_pawn_moves(pos, moves, false, true);
    
    Bitboard attackers = pos.occupied(us) & ~(pos.occupied(us, PAWN) | pos.occupied(us, KING));
    while (attackers) {
        Square from = bitboard_lsb(attackers);
        attackers = bitboard_pop_lsb(attackers);
        Piece p = pos.piece_at(from);
        
        switch (type_of(p)) {
            case KNIGHT:
                generate_knight_moves(pos, moves, from, empty);
                break;
            case BISHOP:
                generate_bishop_moves(pos, moves, from, empty);
                break;
            case ROOK:
                generate_rook_moves(pos, moves, from, empty);
                break;
            case QUEEN:
                generate_queen_moves(pos, moves, from, empty);
                break;
            default:
                break;
        }
    }
    
    // King quiet moves
    Square ksq = SQ_NONE;
    for (int sq = 0; sq < 64; ++sq) {
        if (pos.piece_at(static_cast<Square>(sq)) == make_piece(us, KING)) {
            ksq = static_cast<Square>(sq);
            break;
        }
    }
    if (ksq != SQ_NONE) {
        generate_king_moves(pos, moves, ksq, empty);
    }
    
    // Filter illegal
    MoveList legal;
    for (int i = 0; i < moves.size(); ++i) {
        if (is_legal_quick(const_cast<Position&>(pos), moves[i])) {
            legal.add(moves[i]);
        }
    }
    moves = legal;
}

void MoveGenerator::generate_evasions(const Position& pos, MoveList& moves) {
    moves.clear();
    Color us = pos.side_to_move();
    Color them = !us;
    Bitboard their_pieces = pos.occupied(them);
    Bitboard empty = ~pos.occupied();
    
    // Find king
    Square ksq = SQ_NONE;
    for (int sq = 0; sq < 64; ++sq) {
        if (pos.piece_at(static_cast<Square>(sq)) == make_piece(us, KING)) {
            ksq = static_cast<Square>(sq);
            break;
        }
    }
    
    Bitboard checkers = pos.checkers();
    
    if (bitboard_count(checkers) > 1) {
        // Double check - only king moves
        generate_king_moves(pos, moves, ksq, their_pieces | empty);
    } else {
        Square checker_sq = bitboard_lsb(checkers);
        Piece checker = pos.piece_at(checker_sq);
        
        // Squares between checker and king (for sliding pieces)
        Bitboard blocking_sqs = 0;
        if (type_of(checker) == KNIGHT || type_of(checker) == KING || type_of(checker) == PAWN) {
            blocking_sqs = (1ULL << checker_sq);
        } else {
            blocking_sqs = LINE_BB[ksq][checker_sq] & ~((1ULL << ksq) | (1ULL << checker_sq));
            blocking_sqs |= (1ULL << checker_sq);
        }
        
        // King moves
        generate_king_moves(pos, moves, ksq, their_pieces);
        
        // Pawn moves that block or capture
        generate_pawn_moves(pos, moves, false, false);
        
        // Knight moves that block or capture
        Bitboard knights = pos.occupied(us, KNIGHT);
        while (knights) {
            Square from = bitboard_lsb(knights);
            knights = bitboard_pop_lsb(knights);
            generate_knight_moves(pos, moves, from, (their_pieces | blocking_sqs) & pos.occupied(us, KNIGHT));
        }
        
        // Sliding moves that block or capture
        Bitboard sliders = pos.occupied(us) & (pos.occupied(BISHOP) | pos.occupied(ROOK) | pos.occupied(QUEEN));
        while (sliders) {
            Square from = bitboard_lsb(sliders);
            sliders = bitboard_pop_lsb(sliders);
            Piece p = pos.piece_at(from);
            
            Bitboard targets = their_pieces | blocking_sqs;
            if (type_of(p) == BISHOP || type_of(p) == QUEEN) {
                generate_bishop_moves(pos, moves, from, targets);
            }
            if (type_of(p) == ROOK || type_of(p) == QUEEN) {
                generate_rook_moves(pos, moves, from, targets);
            }
        }
    }
    
    // Filter illegal
    MoveList legal;
    for (int i = 0; i < moves.size(); ++i) {
        if (is_legal_quick(const_cast<Position&>(pos), moves[i])) {
            legal.add(moves[i]);
        }
    }
    moves = legal;
}

void MoveGenerator::generate_pawn_moves(const Position& pos, MoveList& moves, bool captures_only, bool quiets_only) {
    Color us = pos.side_to_move();
    Color them = !us;
    int direction = (us == WHITE) ? 1 : -1;
    int start_rank = (us == WHITE) ? RANK_2 : RANK_7;
    int promo_rank = (us == WHITE) ? RANK_7 : RANK_2;
    
    Bitboard pawns = pos.occupied(us, PAWN);
    Bitboard their_pieces = pos.occupied(them);
    Bitboard empty = ~pos.occupied();
    
    while (pawns) {
        Square from = bitboard_lsb(pawns);
        pawns = bitboard_pop_lsb(pawns);
        
        File f = square_file(from);
        Rank r = square_rank(from);
        
        // Captures
        if (!quiets_only) {
            if (f > FILE_A) {
                Square to = make_square(static_cast<File>(f - 1), static_cast<Rank>(r + direction));
                if (their_pieces & (1ULL << to)) {
                    if (r == promo_rank) {
                        moves.add(Move(from, to, MOVE_PROMOTION, QUEEN));
                        moves.add(Move(from, to, MOVE_PROMOTION, ROOK));
                        moves.add(Move(from, to, MOVE_PROMOTION, BISHOP));
                        moves.add(Move(from, to, MOVE_PROMOTION, KNIGHT));
                    } else {
                        moves.add(Move(from, to));
                    }
                }
            }
            if (f < FILE_H) {
                Square to = make_square(static_cast<File>(f + 1), static_cast<Rank>(r + direction));
                if (their_pieces & (1ULL << to)) {
                    if (r == promo_rank) {
                        moves.add(Move(from, to, MOVE_PROMOTION, QUEEN));
                        moves.add(Move(from, to, MOVE_PROMOTION, ROOK));
                        moves.add(Move(from, to, MOVE_PROMOTION, BISHOP));
                        moves.add(Move(from, to, MOVE_PROMOTION, KNIGHT));
                    } else {
                        moves.add(Move(from, to));
                    }
                }
            }
            
            // En passant
            if (pos.ep_square() != SQ_NONE) {
                Square ep = pos.ep_square();
                if (ep == make_square(static_cast<File>(f - 1), static_cast<Rank>(r + direction))) {
                    moves.add(Move(from, ep, MOVE_EN_PASSANT));
                } else if (ep == make_square(static_cast<File>(f + 1), static_cast<Rank>(r + direction))) {
                    moves.add(Move(from, ep, MOVE_EN_PASSANT));
                }
            }
        }
        
        // Quiet moves
        if (!captures_only) {
            Square push = make_square(f, static_cast<Rank>(r + direction));
            if (empty & (1ULL << push)) {
                if (r == promo_rank) {
                    moves.add(Move(push, push, MOVE_PROMOTION, QUEEN));
                    moves.add(Move(push, push, MOVE_PROMOTION, ROOK));
                    moves.add(Move(push, push, MOVE_PROMOTION, BISHOP));
                    moves.add(Move(push, push, MOVE_PROMOTION, KNIGHT));
                } else {
                    moves.add(Move(from, push));
                    
                    // Double push
                    if (r == start_rank) {
                        Square double_push = make_square(f, static_cast<Rank>(r + 2 * direction));
                        if (empty & (1ULL << double_push)) {
                            moves.add(Move(from, double_push));
                        }
                    }
                }
            }
        }
    }
}

void MoveGenerator::generate_knight_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets) {
    Bitboard attacks = KNIGHT_ATTACKS[from] & targets;
    while (attacks) {
        Square to = bitboard_lsb(attacks);
        attacks = bitboard_pop_lsb(attacks);
        moves.add(Move(from, to));
    }
}

void MoveGenerator::generate_bishop_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets) {
    // Simplified sliding attack generation
    const Direction dirs[] = {NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST};
    
    for (int d = 0; d < 4; ++d) {
        Square s = from;
        while (true) {
            int f = square_file(s) + (dirs[d] == NORTH_EAST || dirs[d] == SOUTH_EAST ? 1 : -1);
            int r = square_rank(s) + (dirs[d] == NORTH_EAST || dirs[d] == NORTH_WEST ? 1 : -1);
            
            if (f < 0 || f > 7 || r < 0 || r > 7) break;
            
            s = make_square(static_cast<File>(f), static_cast<Rank>(r));
            
            if (targets & (1ULL << s)) {
                moves.add(Move(from, s));
            }
            
            if (pos.occupied() & (1ULL << s)) break;
        }
    }
}

void MoveGenerator::generate_rook_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets) {
    const Direction dirs[] = {NORTH, SOUTH, EAST, WEST};
    
    for (int d = 0; d < 4; ++d) {
        Square s = from;
        while (true) {
            int f = square_file(s) + (dirs[d] == EAST ? 1 : dirs[d] == WEST ? -1 : 0);
            int r = square_rank(s) + (dirs[d] == NORTH ? 1 : dirs[d] == SOUTH ? -1 : 0);
            
            if (f < 0 || f > 7 || r < 0 || r > 7) break;
            
            s = make_square(static_cast<File>(f), static_cast<Rank>(r));
            
            if (targets & (1ULL << s)) {
                moves.add(Move(from, s));
            }
            
            if (pos.occupied() & (1ULL << s)) break;
        }
    }
}

void MoveGenerator::generate_queen_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets) {
    generate_bishop_moves(pos, moves, from, targets);
    generate_rook_moves(pos, moves, from, targets);
}

void MoveGenerator::generate_king_moves(const Position& pos, MoveList& moves, Square from, Bitboard targets) {
    Bitboard attacks = KING_ATTACKS[from] & targets;
    while (attacks) {
        Square to = bitboard_lsb(attacks);
        attacks = bitboard_pop_lsb(attacks);
        moves.add(Move(from, to));
    }
}

bool MoveGenerator::is_legal(const Position& pos, Move m) {
    return is_legal_quick(const_cast<Position&>(const_cast<Position&>(pos)), m);
}
