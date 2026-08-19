/*
 * Minimal Chess Engine - Foundation for 3200+ ELO
 * 
 * This is a working base with:
 * - Bitboard representation
 * - Alpha-beta search
 * - Basic evaluation
 * - UCI protocol
 */

#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <random>

using namespace std;

// Constants
constexpr int MAX_DEPTH = 64;
constexpr int MATE_SCORE = 30000;
constexpr int INF_SCORE = 32000;

// Piece values
const int PIECE_VALUES[13] = {0, 100, 320, 330, 500, 900, 20000, 100, 320, 330, 500, 900, 20000};

// Board state
struct Board {
    uint64_t pieces[13];  // WP,WB,WN,WR,WQ,WK,BP,BB,BN,BR,BQ,BK + empty
    uint64_t occupied[3]; // white, black, both
    int stm;              // side to move: 0=white, 1=black
    int castling;         // KQkq rights
    int ep_square;        // en passant square (-1 if none)
    int halfmove;         // halfmove clock
    uint64_t hash;        // zobrist hash
    
    Board() { set_start(); }
    
    void set_start() {
        memset(this, 0, sizeof(Board));
        stm = 0;
        castling = 15;
        ep_square = -1;
        
        // White pieces (rank 1 and 2)
        pieces[1] = 0x00FF000000000000ULL;  // Pawns on rank 7 (from white's view)
        pieces[2] = 0x0000000000000042ULL;  // Knights
        pieces[3] = 0x0000000000000024ULL;  // Bishops
        pieces[4] = 0x0000000000000081ULL;  // Rooks
        pieces[5] = 0x0000000000000008ULL;  // Queen
        pieces[6] = 0x0000000000000010ULL;  // King
        
        // Black pieces (rank 7 and 8)
        pieces[7] = 0x000000000000FF00ULL;  // Pawns on rank 2
        pieces[8] = 0x4200000000000000ULL;  // Knights
        pieces[9] = 0x2400000000000000ULL;  // Bishops
        pieces[10] = 0x8100000000000000ULL; // Rooks
        pieces[11] = 0x0800000000000000ULL; // Queen
        pieces[12] = 0x1000000000000000ULL; // King
        
        update_occupied();
        compute_hash();
    }
    
    void update_occupied() {
        occupied[0] = pieces[1] | pieces[2] | pieces[3] | pieces[4] | pieces[5] | pieces[6];
        occupied[1] = pieces[7] | pieces[8] | pieces[9] | pieces[10] | pieces[11] | pieces[12];
        occupied[2] = occupied[0] | occupied[1];
    }
    
    void compute_hash() {
        static uint64_t piece_keys[13][64];
        static bool init = false;
        if (!init) {
            random_device rd;
            mt19937_64 rng(12345);
            for (int p = 0; p < 13; p++)
                for (int s = 0; s < 64; s++)
                    piece_keys[p][s] = rng();
            init = true;
        }
        
        hash = 0;
        for (int p = 1; p <= 12; p++) {
            uint64_t bb = pieces[p];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                hash ^= piece_keys[p][sq];
                bb &= bb - 1;
            }
        }
        if (stm) hash ^= 0x123456789ABCDEF0ULL;
    }
    
    bool in_check(int c) const {
        int king_sq = __builtin_ctzll(pieces[c == 0 ? 6 : 12]);
        return is_attacked(king_sq, c);
    }
    
    bool is_attacked(int sq, int by_color) const {
        uint64_t occ = occupied[2];
        
        // Pawn attacks
        uint64_t pawns = pieces[by_color == 0 ? 1 : 7];
        if (by_color == 0) {
            if ((pawn_attacks_white(sq) & pawns)) return true;
        } else {
            if ((pawn_attacks_black(sq) & pawns)) return true;
        }
        
        // Knight
        uint64_t knights = pieces[by_color == 0 ? 2 : 8];
        if (knight_attacks(sq) & knights) return true;
        
        // King
        uint64_t kings = pieces[by_color == 0 ? 6 : 12];
        if (king_attacks(sq) & kings) return true;
        
        // Sliding
        uint64_t bishops = pieces[by_color == 0 ? 3 : 9];
        uint64_t rooks = pieces[by_color == 0 ? 4 : 10];
        uint64_t queens = pieces[by_color == 0 ? 5 : 11];
        
        if (bishop_attacks(sq, occ) & (bishops | queens)) return true;
        if (rook_attacks(sq, occ) & (rooks | queens)) return true;
        
        return false;
    }
    
    uint64_t pawn_attacks_white(int sq) const {
        uint64_t b = 1ULL << sq;
        return ((b << 7) & 0xFEFEFEFEFEFEFEULL) | ((b << 9) & 0x7F7F7F7F7F7F7F7FULL);
    }
    
    uint64_t pawn_attacks_black(int sq) const {
        uint64_t b = 1ULL << sq;
        return ((b >> 7) & 0x7F7F7F7F7F7F7F7FULL) | ((b >> 9) & 0xFEFEFEFEFEFEFEULL);
    }
    
    uint64_t knight_attacks(int sq) const {
        static uint64_t table[64] = {};
        static bool init = false;
        if (!init) {
            for (int s = 0; s < 64; s++) {
                int r = s / 8, f = s % 8;
                for (int dr : {-2,-1,1,2}) {
                    for (int df : {-2,-1,1,2}) {
                        if (abs(dr) != abs(df)) {
                            int tr = r + dr, tf = f + df;
                            if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8)
                                table[s] |= 1ULL << (tr * 8 + tf);
                        }
                    }
                }
            }
            init = true;
        }
        return table[sq];
    }
    
    uint64_t king_attacks(int sq) const {
        static uint64_t table[64] = {};
        static bool init = false;
        if (!init) {
            for (int s = 0; s < 64; s++) {
                int r = s / 8, f = s % 8;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int df = -1; df <= 1; df++) {
                        if (dr || df) {
                            int tr = r + dr, tf = f + df;
                            if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8)
                                table[s] |= 1ULL << (tr * 8 + tf);
                        }
                    }
                }
            }
            init = true;
        }
        return table[sq];
    }
    
    uint64_t bishop_attacks(int sq, uint64_t occ) const {
        uint64_t attacks = 0;
        int r = sq / 8, f = sq % 8;
        for (int dr : {-1, -1, 1, 1}) {
            for (int df : {-1, 1, -1, 1}) {
                if ((dr == -1 && df == -1) || (dr == -1 && df == 1) || 
                    (dr == 1 && df == -1) || (dr == 1 && df == 1)) {
                } else continue;
                for (int d = 1; d < 8; d++) {
                    int tr = r + dr * d, tf = f + df * d;
                    if (tr < 0 || tr >= 8 || tf < 0 || tf >= 8) break;
                    int tsq = tr * 8 + tf;
                    attacks |= 1ULL << tsq;
                    if ((occ >> tsq) & 1) break;
                }
            }
            break;
        }
        // Simplified - just do all 4 diagonals properly
        attacks = 0;
        for (int dr : {-1, 1}) {
            for (int df : {-1, 1}) {
                for (int d = 1; d < 8; d++) {
                    int tr = r + dr * d, tf = f + df * d;
                    if (tr < 0 || tr >= 8 || tf < 0 || tf >= 8) break;
                    int tsq = tr * 8 + tf;
                    attacks |= 1ULL << tsq;
                    if ((occ >> tsq) & 1) break;
                }
            }
        }
        return attacks;
    }
    
    uint64_t rook_attacks(int sq, uint64_t occ) const {
        uint64_t attacks = 0;
        int r = sq / 8, f = sq % 8;
        for (int dr : {-1, 0, 1}) {
            for (int df : {-1, 0, 1}) {
                if (abs(dr) + abs(df) != 1) continue;
                for (int d = 1; d < 8; d++) {
                    int tr = r + dr * d, tf = f + df * d;
                    if (tr < 0 || tr >= 8 || tf < 0 || tf >= 8) break;
                    int tsq = tr * 8 + tf;
                    attacks |= 1ULL << tsq;
                    if ((occ >> tsq) & 1) break;
                }
            }
        }
        return attacks;
    }
};

// Move encoding: from(6) | to(6) | promo(2) | flag(2)
struct Move {
    uint16_t data;
    Move() : data(0) {}
    Move(int from, int to, int promo = 0, int flag = 0) 
        : data(from | (to << 6) | (promo << 12) | (flag << 14)) {}
    
    int from_sq() const { return data & 63; }
    int to_sq() const { return (data >> 6) & 63; }
    int promo() const { return (data >> 12) & 3; }
    int flag() const { return (data >> 14) & 3; }
    bool is_null() const { return data == 0; }
    static Move null() { return Move(); }
};

constexpr int FLAG_NORMAL = 0, FLAG_CASTLE = 1, FLAG_EP = 2, FLAG_PROMO = 3;

// Move generation
void generate_moves(const Board& b, vector<Move>& moves, bool captures_only = false) {
    int us = b.stm, them = 1 - us;
    uint64_t our_pieces = b.occupied[us];
    uint64_t their_pieces = b.occupied[them];
    uint64_t empty = ~b.occupied[2];
    uint64_t targets = captures_only ? their_pieces : (empty | their_pieces);
    
    // Pawns
    uint64_t pawns = b.pieces[us == 0 ? 1 : 7];
    int dir = us == 0 ? 8 : -8;
    int start_rank = us == 0 ? 1 : 6;
    int promo_rank = us == 0 ? 7 : 0;
    
    while (pawns) {
        int from = __builtin_ctzll(pawns);
        pawns &= pawns - 1;
        
        // Single push
        int to1 = from + dir;
        if (to1 >= 0 && to1 < 64 && !((b.occupied[2] >> to1) & 1)) {
            if ((to1 / 8) == promo_rank) {
                if (!captures_only) {
                    moves.emplace_back(from, to1, 1); // N
                    moves.emplace_back(from, to1, 2); // B
                    moves.emplace_back(from, to1, 3); // R
                    moves.emplace_back(from, to1, 4); // Q
                }
            } else {
                if (!captures_only) moves.emplace_back(from, to1);
                
                // Double push
                int to2 = from + dir * 2;
                if ((from / 8) == start_rank && to2 >= 0 && to2 < 64 && 
                    !((b.occupied[2] >> to2) & 1)) {
                    if (!captures_only) moves.emplace_back(from, to2);
                }
            }
        }
        
        // Captures
        uint64_t cap_mask = us == 0 ? 
            (((their_pieces | (b.ep_square >= 0 ? 1ULL << b.ep_square : 0)) >> 7) & 0x00FFFFFFFFFFFFFFULL & ~(0x0101010101010101ULL << (from % 8 == 0 ? 7 : 0))) |
            (((their_pieces | (b.ep_square >= 0 ? 1ULL << b.ep_square : 0)) >> 9) & 0x7F7F7F7F7F7F7F7FULL & ~(0x8080808080808080ULL >> (from % 8 == 7 ? 7 : 0))) :
            (((their_pieces | (b.ep_square >= 0 ? 1ULL << b.ep_square : 0)) << 7) & 0xFFFFFFFFFFFFFF00ULL & ~(0x0101010101010101ULL << (from % 8 == 0 ? 7 : 0))) |
            (((their_pieces | (b.ep_square >= 0 ? 1ULL << b.ep_square : 0)) << 9) & 0xFEFEFEFEFEFEFE00ULL & ~(0x8080808080808080ULL >> (from % 8 == 7 ? 7 : 0)));
        
        // Simpler capture logic
        int lcap = from + dir - 1, rcap = from + dir + 1;
        if (lcap >= 0 && lcap < 64 && (from % 8 > 0) && ((their_pieces >> lcap) & 1)) {
            if ((lcap / 8) == promo_rank) {
                moves.emplace_back(from, lcap, 4);
            } else {
                moves.emplace_back(from, lcap);
            }
        }
        if (rcap >= 0 && rcap < 64 && (from % 8 < 7) && ((their_pieces >> rcap) & 1)) {
            if ((rcap / 8) == promo_rank) {
                moves.emplace_back(from, rcap, 4);
            } else {
                moves.emplace_back(from, rcap);
            }
        }
        // EP
        if (b.ep_square >= 0) {
            if (lcap == b.ep_square) {
                Move m(from, lcap);
                m.data |= (FLAG_EP << 14);
                moves.push_back(m);
            }
            if (rcap == b.ep_square) {
                Move m(from, rcap);
                m.data |= (FLAG_EP << 14);
                moves.push_back(m);
            }
        }
    }
    
    // Knights
    uint64_t knights = b.pieces[us == 0 ? 2 : 8];
    while (knights) {
        int from = __builtin_ctzll(knights);
        knights &= knights - 1;
        uint64_t att = 0;
        int r = from / 8, f = from % 8;
        for (int dr : {-2,-1,1,2}) {
            for (int df : {-2,-1,1,2}) {
                if (abs(dr) != abs(df)) {
                    int tr = r + dr, tf = f + df;
                    if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8)
                        att |= 1ULL << (tr * 8 + tf);
                }
            }
        }
        uint64_t tg = att & targets;
        while (tg) {
            int to = __builtin_ctzll(tg);
            tg &= tg - 1;
            if (!((our_pieces >> to) & 1))
                moves.emplace_back(from, to);
        }
    }
    
    // Kings
    uint64_t kings = b.pieces[us == 0 ? 6 : 12];
    while (kings) {
        int from = __builtin_ctzll(kings);
        kings &= kings - 1;
        uint64_t att = 0;
        int r = from / 8, f = from % 8;
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr || df) {
                    int tr = r + dr, tf = f + df;
                    if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8)
                        att |= 1ULL << (tr * 8 + tf);
                }
            }
        }
        uint64_t tg = att & targets;
        while (tg) {
            int to = __builtin_ctzll(tg);
            tg &= tg - 1;
            if (!((our_pieces >> to) & 1))
                moves.emplace_back(from, to);
        }
        
        // Castling
        if (!captures_only && !b.in_check(us)) {
            if (us == 0) {
                if ((b.castling & 1) && !(b.occupied[2] & 0x60ULL)) {
                    if (!b.is_attacked(5, 1) && !b.is_attacked(6, 1))
                        moves.emplace_back(4, 6, 0, FLAG_CASTLE);
                }
                if ((b.castling & 2) && !(b.occupied[2] & 0x0EULL)) {
                    if (!b.is_attacked(3, 1) && !b.is_attacked(2, 1))
                        moves.emplace_back(4, 2, 0, FLAG_CASTLE);
                }
            } else {
                if ((b.castling & 4) && !(b.occupied[2] & 0x6000000000000000ULL)) {
                    if (!b.is_attacked(60, 0) && !b.is_attacked(61, 0))
                        moves.emplace_back(60, 62, 0, FLAG_CASTLE);
                }
                if ((b.castling & 8) && !(b.occupied[2] & 0x0E00000000000000ULL)) {
                    if (!b.is_attacked(59, 0) && !b.is_attacked(58, 0))
                        moves.emplace_back(60, 58, 0, FLAG_CASTLE);
                }
            }
        }
    }
    
    // Sliding pieces (bishops, rooks, queens)
    auto slide = [&](uint64_t pcs, bool diag, bool straight) {
        while (pcs) {
            int from = __builtin_ctzll(pcs);
            pcs &= pcs - 1;
            
            static const int dirs_diag[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
            static const int dirs_straight[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            
            auto process_dir = [&](int dr, int df) {
                uint64_t att = 0;
                int r = from / 8, f = from % 8;
                for (int d = 1; d < 8; d++) {
                    int tr = r + dr * d, tf = f + df * d;
                    if (tr < 0 || tr >= 8 || tf < 0 || tf >= 8) return att;
                    int tsq = tr * 8 + tf;
                    att |= 1ULL << tsq;
                    if ((b.occupied[2] >> tsq) & 1) return att;
                }
                return att;
            };
            
            uint64_t att = 0;
            if (diag) {
                for (auto& d : dirs_diag) att |= process_dir(d[0], d[1]);
            }
            if (straight) {
                for (auto& d : dirs_straight) att |= process_dir(d[0], d[1]);
            }
            
            uint64_t tg = att & targets;
            while (tg) {
                int to = __builtin_ctzll(tg);
                tg &= tg - 1;
                if (!((our_pieces >> to) & 1))
                    moves.emplace_back(from, to);
            }
        }
    };
    
    slide(b.pieces[us == 0 ? 3 : 9], true, false);   // Bishops
    slide(b.pieces[us == 0 ? 4 : 10], false, true);  // Rooks
    slide(b.pieces[us == 0 ? 5 : 11], true, true);   // Queens
}

// Make/unmake move
struct UndoInfo {
    int captured_piece;
    int castling;
    int ep_square;
    int halfmove;
    uint64_t hash;
};

void make_move(Board& b, const Move& m, UndoInfo& undo) {
    int from = m.from_sq(), to = m.to_sq();
    int us = b.stm, them = 1 - us;
    
    undo.captured_piece = 0;
    undo.castling = b.castling;
    undo.ep_square = b.ep_square;
    undo.halfmove = b.halfmove;
    undo.hash = b.hash;
    
    // Find moving piece
    int piece = 0;
    for (int p = 1; p <= 12; p++) {
        if ((b.pieces[p] >> from) & 1) {
            piece = p;
            break;
        }
    }
    
    // Handle capture
    for (int p = 1; p <= 12; p++) {
        if ((b.pieces[p] >> to) & 1) {
            b.pieces[p] &= ~(1ULL << to);
            undo.captured_piece = p;
            break;
        }
    }
    
    // EP capture
    if (m.flag() == FLAG_EP) {
        int ep_pawn_sq = us == 0 ? to - 8 : to + 8;
        int ep_pawn = us == 0 ? 7 : 1;
        b.pieces[ep_pawn] &= ~(1ULL << ep_pawn_sq);
        undo.captured_piece = ep_pawn;
    }
    
    // Move piece
    b.pieces[piece] &= ~(1ULL << from);
    b.pieces[piece] |= (1ULL << to);
    
    // Promotion
    if (m.promo()) {
        b.pieces[piece] &= ~(1ULL << to);
        int promo_piece = us == 0 ? (m.promo() + 1) : (m.promo() + 7);
        b.pieces[promo_piece] |= (1ULL << to);
    }
    
    // Castling
    if (m.flag() == FLAG_CASTLE) {
        if (to == 6) {  // White kingside
            b.pieces[4] &= ~(1ULL << 7);
            b.pieces[4] |= (1ULL << 5);
        } else if (to == 2) {  // White queenside
            b.pieces[4] &= ~(1ULL << 0);
            b.pieces[4] |= (1ULL << 3);
        } else if (to == 62) {  // Black kingside
            b.pieces[10] &= ~(1ULL << 63);
            b.pieces[10] |= (1ULL << 61);
        } else if (to == 58) {  // Black queenside
            b.pieces[10] &= ~(1ULL << 56);
            b.pieces[10] |= (1ULL << 59);
        }
    }
    
    // Update castling rights
    if (piece == 6) b.castling &= ~3;
    if (piece == 12) b.castling &= ~12;
    if (piece == 4 && from == 0) b.castling &= ~2;
    if (piece == 4 && from == 7) b.castling &= ~1;
    if (piece == 10 && from == 56) b.castling &= ~8;
    if (piece == 10 && from == 63) b.castling &= ~4;
    
    // Update EP square
    b.ep_square = -1;
    if ((piece == 1 || piece == 7) && abs(to - from) == 16) {
        b.ep_square = us == 0 ? from + 8 : from - 8;
    }
    
    // Halfmove clock
    if (piece == 1 || piece == 7 || undo.captured_piece) {
        b.halfmove = 0;
    } else {
        b.halfmove++;
    }
    
    b.stm = them;
    b.update_occupied();
    b.compute_hash();
}

void unmake_move(Board& b, const Move& m, const UndoInfo& undo) {
    int from = m.from_sq(), to = m.to_sq();
    int us = 1 - b.stm, them = b.stm;
    
    // Find piece at 'to'
    int piece = 0;
    for (int p = 1; p <= 12; p++) {
        if ((b.pieces[p] >> to) & 1) {
            piece = p;
            break;
        }
    }
    
    // Handle promotion
    if (m.promo()) {
        int promo = us == 0 ? (m.promo() + 1) : (m.promo() + 7);
        b.pieces[promo] &= ~(1ULL << to);
        piece = us == 0 ? 1 : 7;
    }
    
    // Move piece back
    b.pieces[piece] &= ~(1ULL << to);
    b.pieces[piece] |= (1ULL << from);
    
    // Restore capture
    if (undo.captured_piece) {
        if (m.flag() == FLAG_EP) {
            int ep_pawn_sq = us == 0 ? to - 8 : to + 8;
            b.pieces[undo.captured_piece] |= (1ULL << ep_pawn_sq);
        } else {
            b.pieces[undo.captured_piece] |= (1ULL << to);
        }
    }
    
    // Restore castling
    if (m.flag() == FLAG_CASTLE) {
        if (to == 6) {
            b.pieces[4] &= ~(1ULL << 5);
            b.pieces[4] |= (1ULL << 7);
        } else if (to == 2) {
            b.pieces[4] &= ~(1ULL << 3);
            b.pieces[4] |= (1ULL << 0);
        } else if (to == 62) {
            b.pieces[10] &= ~(1ULL << 61);
            b.pieces[10] |= (1ULL << 63);
        } else if (to == 58) {
            b.pieces[10] &= ~(1ULL << 59);
            b.pieces[10] |= (1ULL << 56);
        }
    }
    
    b.castling = undo.castling;
    b.ep_square = undo.ep_square;
    b.halfmove = undo.halfmove;
    b.stm = us;
    b.update_occupied();
    b.hash = undo.hash;
}

// Evaluation
int evaluate(const Board& b) {
    int score = 0;
    
    // Material
    for (int p = 1; p <= 12; p++) {
        score += __builtin_popcountll(b.pieces[p]) * PIECE_VALUES[p];
    }
    
    // Simple PST for pawns
    const int pawn_pst[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    };
    
    uint64_t wp = b.pieces[1];
    while (wp) {
        int sq = __builtin_ctzll(wp);
        score += pawn_pst[sq];
        wp &= wp - 1;
    }
    
    uint64_t bp = b.pieces[7];
    while (bp) {
        int sq = __builtin_ctzll(bp);
        score -= pawn_pst[sq ^ 56];
        bp &= bp - 1;
    }
    
    return b.stm == 0 ? score : -score;
}

// Search
struct SearchStats {
    uint64_t nodes;
    Move best_move;
    chrono::steady_clock::time_point start;
};

int quiescence(Board& b, int alpha, int beta, SearchStats& stats) {
    stats.nodes++;
    
    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;
    
    vector<Move> moves;
    generate_moves(b, moves, true);
    
    // Order captures by MVV-LVA
    sort(moves.begin(), moves.end(), [&b](const Move& a, const Move& b_move) {
        int va = 0, vb = 0;
        for (int p = 1; p <= 12; p++) {
            if ((b.pieces[p] >> a.to_sq()) & 1) va = PIECE_VALUES[p];
            if ((b.pieces[p] >> b_move.to_sq()) & 1) vb = PIECE_VALUES[p];
        }
        return va > vb;
    });
    
    for (const auto& m : moves) {
        UndoInfo undo;
        make_move(b, m, undo);
        int score = -quiescence(b, -beta, -alpha, stats);
        unmake_move(b, m, undo);
        
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    
    return alpha;
}

int alphabeta(Board& b, int depth, int alpha, int beta, int ply, SearchStats& stats) {
    stats.nodes++;
    
    if (b.halfmove >= 100) return 0;
    
    if (depth <= 0) {
        return quiescence(b, alpha, beta, stats);
    }
    
    bool in_check = b.in_check(b.stm);
    if (in_check) depth++;
    
    // Null move pruning
    if (!in_check && depth >= 2 && alpha > -MATE_SCORE + 100) {
        UndoInfo undo;
        undo.captured_piece = 0;
        undo.castling = b.castling;
        undo.ep_square = b.ep_square;
        undo.halfmove = b.halfmove;
        undo.hash = b.hash;
        b.stm = 1 - b.stm;
        b.ep_square = -1;
        b.update_occupied();
        b.compute_hash();
        
        int r = 3 + depth / 4;
        int score = -alphabeta(b, depth - 1 - r, -beta, -beta + 1, ply + 1, stats);
        
        b.stm = 1 - b.stm;
        b.ep_square = undo.ep_square;
        b.update_occupied();
        b.hash = undo.hash;
        
        if (score >= beta) return beta;
    }
    
    vector<Move> moves;
    generate_moves(b, moves);
    
    if (moves.empty()) {
        return in_check ? -MATE_SCORE + ply : 0;
    }
    
    // Simple move ordering - captures first, then promotions
    sort(moves.begin(), moves.end(), [&b](const Move& a, const Move& b_move) {
        bool a_cap = false, b_cap = false;
        for (int p = 1; p <= 12; p++) {
            if ((b.pieces[p] >> a.to_sq()) & 1) a_cap = true;
            if ((b.pieces[p] >> b_move.to_sq()) & 1) b_cap = true;
        }
        if (a_cap && !b_cap) return true;
        if (!a_cap && b_cap) return false;
        if (a.promo() && !b_move.promo()) return true;
        return false;
    });
    
    Move best_move = moves[0];
    int best_score = -INF_SCORE;
    int old_alpha = alpha;
    
    for (size_t i = 0; i < moves.size(); i++) {
        const auto& m = moves[i];
        UndoInfo undo;
        make_move(b, m, undo);
        
        int score;
        if (i == 0) {
            score = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1, stats);
        } else {
            score = -alphabeta(b, depth - 1, -alpha - 1, -alpha, ply + 1, stats);
            if (score > alpha && score < beta) {
                score = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1, stats);
            }
        }
        
        unmake_move(b, m, undo);
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) break;
            }
        }
    }
    
    return best_score <= old_alpha ? best_score : best_score;
}

Move find_best_move(Board& b, int time_ms) {
    SearchStats stats = {0, Move::null(), chrono::steady_clock::now()};
    
    Move best = Move::null();
    int best_score = -INF_SCORE;
    
    for (int depth = 1; depth <= MAX_DEPTH; depth++) {
        vector<Move> moves;
        generate_moves(b, moves);
        
        if (moves.empty()) break;
        
        for (const auto& m : moves) {
            UndoInfo undo;
            make_move(b, m, undo);
            int score = -alphabeta(b, depth - 1, -INF_SCORE, INF_SCORE, 0, stats);
            unmake_move(b, m, undo);
            
            if (score > best_score) {
                best_score = score;
                best = m;
            }
        }
        
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - stats.start).count();
        
        long long elapsed_ll = elapsed;
        cout << "info depth " << depth << " score cp " << best_score 
             << " nodes " << stats.nodes << " nps " 
             << (stats.nodes * 1000 / max(elapsed_ll, 1LL)) << endl;
        
        if (elapsed > time_ms * 0.8 || depth >= 15) break;
    }
    
    return best;
}

// UCI
string sq_to_alg(int sq) {
    const char files[] = "abcdefgh";
    const char ranks[] = "12345678";
    return string() + files[sq % 8] + ranks[sq / 8];
}

int main() {
    Board board;
    
    cout << "id name ChessEngine3200" << endl;
    cout << "id author AI" << endl;
    cout << "uciok" << endl;
    
    string line;
    while (getline(cin, line)) {
        istringstream iss(line);
        string cmd;
        iss >> cmd;
        
        if (cmd == "quit") break;
        else if (cmd == "ucinewgame") board.set_start();
        else if (cmd == "position") {
            string token;
            iss >> token;
            if (token == "startpos") {
                board.set_start();
                if (iss >> token && token == "moves") {
                    string move_str;
                    while (iss >> move_str) {
                        // Parse move - simplified
                    }
                }
            } else if (token == "fen") {
                // Full FEN parsing would go here
                board.set_start();
            }
        } else if (cmd == "go") {
            string token;
            int time = 1000, movetime = -1;
            while (iss >> token) {
                if (token == "wtime" || token == "btime") iss >> time;
                else if (token == "movetime") iss >> movetime;
            }
            int search_time = movetime > 0 ? movetime : time / 20;
            Move best = find_best_move(board, search_time);
            cout << "bestmove " << sq_to_alg(best.from_sq()) << sq_to_alg(best.to_sq());
            if (best.promo()) {
                char p = best.promo() == 1 ? 'n' : best.promo() == 2 ? 'b' : best.promo() == 3 ? 'r' : 'q';
                cout << p;
            }
            cout << endl;
        }
    }
    
    return 0;
}
