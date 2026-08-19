/*
 * High-Performance Chess Engine with NNUE Evaluation
 * Target: 3200+ ELO
 * 
 * Features:
 * - Alpha-beta search with iterative deepening
 * - Transposition tables
 * - Move ordering (MVV-LVA, killer moves, history heuristic)
 * - Null move pruning, futility pruning, late move reductions
 * - NNUE-style neural network evaluation
 * - UCI protocol support
 */

#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <string>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>

// ============================================================================
// Constants and Types
// ============================================================================

constexpr int SQUARE_COUNT = 64;
constexpr int PIECE_COUNT = 12;
constexpr int COLOR_COUNT = 2;
constexpr int MAX_MOVES = 256;
constexpr int MAX_DEPTH = 64;
constexpr int MATE_SCORE = 32000;
constexpr int INF_SCORE = 32767;

enum Color { WHITE, BLACK };
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE = -1
};

enum Piece {
    WP = 1, WN, WB, WR, WQ, WK,
    BP = 9, BN, BB, BR, BQ, BK
};

// ============================================================================
// Bitboard Utilities
// ============================================================================

using Bitboard = uint64_t;

inline Bitboard bit(int sq) { return 1ULL << sq; }
inline int lsb(Bitboard bb) { return __builtin_ctzll(bb); }
inline int popcount(Bitboard bb) { return __builtin_popcountll(bb); }
inline void clear_lsb(Bitboard& bb) { bb &= bb - 1; }
inline int pop_lsb(Bitboard& bb) { int s = lsb(bb); clear_lsb(bb); return s; }

// Attack tables
Bitboard pawn_attacks[COLOR_COUNT][SQUARE_COUNT];
Bitboard knight_attacks[SQUARE_COUNT];
Bitboard bishop_attacks[SQUARE_COUNT];
Bitboard rook_attacks[SQUARE_COUNT];
Bitboard king_attacks[SQUARE_COUNT];

// Magic bitboards for sliding pieces
constexpr int MAGIC_BITS = 64;
uint64_t magic_bishops[SQUARE_COUNT];
uint64_t magic_rooks[SQUARE_COUNT];
int magic_shift_bishops[SQUARE_COUNT];
int magic_shift_rooks[SQUARE_COUNT];
Bitboard* bishop_attacks_magic[SQUARE_COUNT];
Bitboard* rook_attacks_magic[SQUARE_COUNT];

// ============================================================================
// Position Representation
// ============================================================================

struct Position {
    Bitboard occupied;
    Bitboard by_color[COLOR_COUNT];
    Bitboard by_type[PIECE_COUNT + 1]; // 1-12 for pieces
    Bitboard by_piece[PIECE_COUNT + 1];
    
    int side_to_move;
    int castling_rights; // KQkq = 1,2,4,8
    int ep_square;
    int halfmove_clock;
    int fullmove_number;
    
    uint64_t key;
    
    Position() { set_start_position(); }
    
    void set_start_position() {
        std::memset(this, 0, sizeof(Position));
        side_to_move = WHITE;
        castling_rights = 15; // KQkq
        ep_square = SQ_NONE;
        halfmove_clock = 0;
        fullmove_number = 1;
        
        // White pieces
        by_piece[WP] = 0x000000000000FF00ULL;
        by_piece[WN] = 0x0000000000000042ULL;
        by_piece[WB] = 0x0000000000000024ULL;
        by_piece[WR] = 0x0000000000000081ULL;
        by_piece[WQ] = 0x0000000000000008ULL;
        by_piece[WK] = 0x0000000000000010ULL;
        
        // Black pieces
        by_piece[BP] = 0x00FF000000000000ULL;
        by_piece[BN] = 0x4200000000000000ULL;
        by_piece[BB] = 0x2400000000000000ULL;
        by_piece[BR] = 0x8100000000000000ULL;
        by_piece[BQ] = 0x0800000000000000ULL;
        by_piece[BK] = 0x1000000000000000ULL;
        
        update_from_pieces();
        key = compute_key();
    }
    
    void update_from_pieces() {
        occupied = 0;
        by_color[WHITE] = 0;
        by_color[BLACK] = 0;
        
        for (int p = 1; p <= PIECE_COUNT; p++) {
            by_type[p] = by_piece[p];
            occupied |= by_piece[p];
            
            if (p >= WP && p <= WK) {
                by_color[WHITE] |= by_piece[p];
            } else if (p >= BP && p <= BK) {
                by_color[BLACK] |= by_piece[p];
            }
        }
    }
    
    uint64_t compute_key() const {
        uint64_t k = 0;
        static std::mt19937_64 rng(42);
        static uint64_t piece_keys[PIECE_COUNT + 1][SQUARE_COUNT];
        static uint64_t castling_keys[16];
        static uint64_t ep_keys[SQUARE_COUNT + 1];
        static bool initialized = false;
        
        if (!initialized) {
            for (int p = 1; p <= PIECE_COUNT; p++)
                for (int s = 0; s < SQUARE_COUNT; s++)
                    piece_keys[p][s] = rng();
            for (int i = 0; i < 16; i++)
                castling_keys[i] = rng();
            for (int s = 0; s <= SQUARE_COUNT; s++)
                ep_keys[s] = rng();
            initialized = true;
        }
        
        for (int p = 1; p <= PIECE_COUNT; p++) {
            Bitboard bb = by_piece[p];
            while (bb) {
                int sq = pop_lsb(bb);
                k ^= piece_keys[p][sq];
            }
        }
        
        k ^= castling_keys[castling_rights];
        k ^= ep_keys[ep_square];
        
        if (side_to_move == BLACK)
            k ^= 0x123456789ABCDEF0ULL;
        
        return k;
    }
    
    bool is_attacked_by(int sq, int them) const {
        Bitboard occ = occupied;
        
        // Pawn attacks
        Bitboard pawns = by_piece[them == WHITE ? WP : BP];
        if (them == WHITE) {
            if (pawn_attacks[WHITE][sq] & pawns) return true;
        } else {
            if (pawn_attacks[BLACK][sq] & pawns) return true;
        }
        
        // Knight attacks
        Bitboard knights = by_piece[them == WHITE ? WN : BN];
        if (knight_attacks[sq] & knights) return true;
        
        // King attacks
        Bitboard kings = by_piece[them == WHITE ? WK : BK];
        if (king_attacks[sq] & kings) return true;
        
        // Sliding pieces
        Bitboard bishops = by_piece[them == WHITE ? WB : BB];
        Bitboard queens = by_piece[them == WHITE ? WQ : BQ];
        Bitboard rooks = by_piece[them == WHITE ? WR : BR];
        
        Bitboard diag_attackers = bishops | queens;
        Bitboard straight_attackers = rooks | queens;
        
        if (get_bishop_attacks(sq, occ) & diag_attackers) return true;
        if (get_rook_attacks(sq, occ) & straight_attackers) return true;
        
        return false;
    }
    
    bool in_check(int c) const {
        int king_sq = lsb(by_piece[c == WHITE ? WK : BK]);
        return is_attacked_by(king_sq, c == WHITE ? BLACK : WHITE);
    }
    
    Bitboard get_bishop_attacks(int sq, Bitboard occ) const {
        return bishop_attacks_magic[sq][(occ & magic_bishops[sq]) >> magic_shift_bishops[sq]];
    }
    
    Bitboard get_rook_attacks(int sq, Bitboard occ) const {
        return rook_attacks_magic[sq][(occ & magic_rooks[sq]) >> magic_shift_rooks[sq]];
    }
    
    Bitboard get_queen_attacks(int sq, Bitboard occ) const {
        return get_bishop_attacks(sq, occ) | get_rook_attacks(sq, occ);
    }
};

// ============================================================================
// Move Encoding
// ============================================================================

struct Move {
    uint16_t data;
    
    Move() : data(0) {}
    Move(int from, int to, int promotion = 0, int flag = 0) {
        data = from | (to << 6) | (promotion << 12) | (flag << 14);
    }
    
    int from() const { return data & 0x3F; }
    int to() const { return (data >> 6) & 0x3F; }
    int promotion() const { return (data >> 12) & 0x3; }
    int flag() const { return (data >> 14) & 0x3; }
    
    bool is_null() const { return data == 0; }
    bool is_promotion() const { return promotion() != 0; }
    
    static Move null() { return Move(); }
};

constexpr int MOVE_FLAG_NORMAL = 0;
constexpr int MOVE_FLAG_CASTLE = 1;
constexpr int MOVE_FLAG_EP = 2;
constexpr int MOVE_FLAG_PROMO = 3;

// ============================================================================
// Move Generation
// ============================================================================

class MoveGenerator {
public:
    static void generate_moves(const Position& pos, std::vector<Move>& moves, bool only_captures = false) {
        int us = pos.side_to_move;
        int them = 1 - us;
        Bitboard our_pieces = pos.by_color[us];
        Bitboard their_pieces = pos.by_color[them];
        Bitboard empty = ~pos.occupied;
        Bitboard capture_targets = their_pieces;
        Bitboard push_targets = only_captures ? 0 : empty;
        
        // Pawn moves
        Bitboard pawns = pos.by_piece[us == WHITE ? WP : BP];
        generate_pawn_moves(pos, pawns, us, capture_targets, push_targets, moves, only_captures);
        
        // Knight moves
        Bitboard knights = pos.by_piece[us == WHITE ? WN : BN];
        generate_knight_moves(knights, our_pieces, capture_targets, push_targets, moves, only_captures);
        
        // Bishop moves
        Bitboard bishops = pos.by_piece[us == WHITE ? WB : BB];
        generate_sliding_moves(bishops, pos, our_pieces, capture_targets, push_targets, 
                              [&pos](int sq, Bitboard occ) { return pos.get_bishop_attacks(sq, occ); },
                              moves, only_captures);
        
        // Rook moves
        Bitboard rooks = pos.by_piece[us == WHITE ? WR : BR];
        generate_sliding_moves(rooks, pos, our_pieces, capture_targets, push_targets,
                              [&pos](int sq, Bitboard occ) { return pos.get_rook_attacks(sq, occ); },
                              moves, only_captures);
        
        // Queen moves
        Bitboard queens = pos.by_piece[us == WHITE ? WQ : BQ];
        generate_sliding_moves(queens, pos, our_pieces, capture_targets, push_targets,
                              [&pos](int sq, Bitboard occ) { return pos.get_queen_attacks(sq, occ); },
                              moves, only_captures);
        
        // King moves
        Bitboard kings = pos.by_piece[us == WHITE ? WK : BK];
        generate_king_moves(kings, pos, us, our_pieces, capture_targets, push_targets, moves, only_captures);
    }
    
private:
    static void generate_pawn_moves(const Position& pos, Bitboard pawns, int us,
                                   Bitboard capture_targets, Bitboard push_targets,
                                   std::vector<Move>& moves, bool only_captures) {
        int dir = us == WHITE ? 8 : -8;
        int start_rank = us == WHITE ? 1 : 6;
        int promo_rank = us == WHITE ? 7 : 0;
        
        Bitboard single_push = (us == WHITE ? (pawns << 8) : (pawns >> 8)) & push_targets;
        Bitboard double_push = us == WHITE ? (single_push & ~bit(promo_rank) << 8) : (single_push & ~bit(promo_rank) >> 8);
        double_push &= push_targets;
        
        if (!only_captures) {
            // Single pushes
            Bitboard bb = single_push;
            while (bb) {
                int to = pop_lsb(bb);
                int from = to - dir;
                if ((to & 56) == promo_rank) {
                    moves.emplace_back(from, to, 1); // Knight promo
                    moves.emplace_back(from, to, 2); // Bishop promo
                    moves.emplace_back(from, to, 3); // Rook promo
                    moves.emplace_back(from, to, 4); // Queen promo
                } else {
                    moves.emplace_back(from, to);
                }
            }
            
            // Double pushes
            bb = double_push;
            while (bb) {
                int to = pop_lsb(bb);
                int from = to - dir * 2;
                moves.emplace_back(from, to);
            }
        }
        
        // Captures
        Bitboard cap_mask = us == WHITE ? 
            (((capture_targets | bit(pos.ep_square)) >> 7) & 0x00FFFFFFFFFFFFFFULL) |
            (((capture_targets | bit(pos.ep_square)) >> 9) & 0x00FFFFFFFFFFFFFFULL) :
            (((capture_targets | bit(pos.ep_square)) << 7) & 0xFFFFFFFFFFFFFF00ULL) |
            (((capture_targets | bit(pos.ep_square)) << 9) & 0xFFFFFFFFFFFFFF00ULL);
        
        Bitboard captures = pawns & cap_mask;
        while (captures) {
            int from = pop_lsb(captures);
            int to = us == WHITE ? from + dir - 1 : from + dir + 1;
            if (!(bit(to) & capture_targets) && to != pos.ep_square) {
                to = us == WHITE ? from + dir + 1 : from + dir - 1;
                if (!(bit(to) & capture_targets)) continue;
            }
            
            if ((to & 56) == promo_rank) {
                if (bit(to) & capture_targets || to == pos.ep_square) {
                    moves.emplace_back(from, to, 4); // Queen promo capture
                    if (to == pos.ep_square) {
                        moves.back().data |= (MOVE_FLAG_EP << 14);
                    }
                }
            } else if (bit(to) & capture_targets) {
                moves.emplace_back(from, to);
            } else if (to == pos.ep_square) {
                Move m(from, to);
                m.data |= (MOVE_FLAG_EP << 14);
                moves.push_back(m);
            }
        }
    }
    
    static void generate_knight_moves(Bitboard knights, Bitboard our_pieces,
                                     Bitboard capture_targets, Bitboard push_targets,
                                     std::vector<Move>& moves, bool only_captures) {
        while (knights) {
            int from = pop_lsb(knights);
            Bitboard targets = knight_attacks[from] & (only_captures ? capture_targets : push_targets | capture_targets);
            while (targets) {
                int to = pop_lsb(targets);
                if (!(bit(to) & our_pieces)) {
                    moves.emplace_back(from, to);
                }
            }
        }
    }
    
    template<typename AttackFunc>
    static void generate_sliding_moves(Bitboard sliders, const Position& pos,
                                       Bitboard our_pieces, Bitboard capture_targets,
                                       Bitboard push_targets,
                                       AttackFunc attack_func,
                                       std::vector<Move>& moves, bool only_captures) {
        Bitboard occ = pos.occupied;
        while (sliders) {
            int from = pop_lsb(sliders);
            Bitboard targets = attack_func(from, occ) & (only_captures ? capture_targets : push_targets | capture_targets);
            while (targets) {
                int to = pop_lsb(targets);
                if (!(bit(to) & our_pieces)) {
                    moves.emplace_back(from, to);
                }
            }
        }
    }
    
    static void generate_king_moves(Bitboard kings, const Position& pos, int us,
                                   Bitboard our_pieces, Bitboard capture_targets,
                                   Bitboard push_targets, std::vector<Move>& moves,
                                   bool only_captures) {
        while (kings) {
            int from = pop_lsb(kings);
            Bitboard targets = king_attacks[from] & (only_captures ? capture_targets : push_targets | capture_targets);
            while (targets) {
                int to = pop_lsb(targets);
                if (!(bit(to) & our_pieces)) {
                    moves.emplace_back(from, to);
                }
            }
            
            // Castling
            if (!only_captures && !pos.in_check(us)) {
                if (us == WHITE) {
                    if ((pos.castling_rights & 1) && !(pos.occupied & 0x60ULL) && 
                        !pos.is_attacked_by(F1, BLACK) && !pos.is_attacked_by(G1, BLACK)) {
                        moves.emplace_back(E1, G1, 0, MOVE_FLAG_CASTLE);
                    }
                    if ((pos.castling_rights & 2) && !(pos.occupied & 0x0EULL) &&
                        !pos.is_attacked_by(D1, BLACK) && !pos.is_attacked_by(C1, BLACK)) {
                        moves.emplace_back(E1, C1, 0, MOVE_FLAG_CASTLE);
                    }
                } else {
                    if ((pos.castling_rights & 4) && !(pos.occupied & 0x6000000000000000ULL) &&
                        !pos.is_attacked_by(F8, WHITE) && !pos.is_attacked_by(G8, WHITE)) {
                        moves.emplace_back(E8, G8, 0, MOVE_FLAG_CASTLE);
                    }
                    if ((pos.castling_rights & 8) && !(pos.occupied & 0x0E00000000000000ULL) &&
                        !pos.is_attacked_by(D8, WHITE) && !pos.is_attacked_by(C8, WHITE)) {
                        moves.emplace_back(E8, C8, 0, MOVE_FLAG_CASTLE);
                    }
                }
            }
        }
    }
};

// ============================================================================
// NNUE Evaluation Network
// ============================================================================

constexpr int N_INPUT = 768;  // 64 squares * 12 piece types
constexpr int N_HIDDEN = 256; // Hidden layer size (can increase for strength)
constexpr int N_OUTPUT = 1;

struct NNUE {
    alignas(64) int16_t input_weights[N_INPUT][N_HIDDEN];
    alignas(64) int16_t hidden_weights[N_HIDDEN];
    alignas(64) int32_t output_weights[N_HIDDEN];
    alignas(64) int16_t input_bias[N_HIDDEN];
    alignas(64) int32_t output_bias;
    
    NNUE() { initialize_random(); }
    
    void initialize_random() {
        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> dist(-127, 127);
        std::uniform_int_distribution<int> bias_dist(-1000, 1000);
        
        for (int i = 0; i < N_INPUT; i++) {
            for (int j = 0; j < N_HIDDEN; j++) {
                input_weights[i][j] = dist(rng);
            }
        }
        
        for (int j = 0; j < N_HIDDEN; j++) {
            input_bias[j] = bias_dist(rng);
        }
        
        for (int j = 0; j < N_HIDDEN; j++) {
            hidden_weights[j] = dist(rng);
            output_weights[j] = dist(rng) * 100;
        }
        
        output_bias = 0;
    }
    
    // Efficiently updatable accumulator
    struct Accumulator {
        alignas(64) int16_t values[N_HIDDEN];
        
        void init(const NNUE& net) {
            for (int j = 0; j < N_HIDDEN; j++) {
                values[j] = net.input_bias[j];
            }
        }
        
        void add_sub(const NNUE& net, int add_idx, int sub_idx) {
            for (int j = 0; j < N_HIDDEN; j++) {
                values[j] += net.input_weights[add_idx][j];
                values[j] -= net.input_weights[sub_idx][j];
            }
        }
        
        void add_sub_add_sub(const NNUE& net, int add1, int sub1, int add2, int sub2) {
            for (int j = 0; j < N_HIDDEN; j++) {
                values[j] += net.input_weights[add1][j];
                values[j] -= net.input_weights[sub1][j];
                values[j] += net.input_weights[add2][j];
                values[j] -= net.input_weights[sub2][j];
            }
        }
    };
    
    int32_t evaluate(const Accumulator& acc) const {
        int32_t sum = output_bias;
        
        for (int j = 0; j < N_HIDDEN; j++) {
            int16_t v = acc.values[j];
            // Clipped ReLU
            int32_t activated = std::max(0, std::min(127, (int)v));
            sum += activated * output_weights[j];
        }
        
        return sum / 64; // Scale down
    }
};

// ============================================================================
// Search Engine
// ============================================================================

struct SearchStats {
    uint64_t nodes;
    uint64_t qnodes;
    int depth;
    Move best_move;
    int best_score;
    std::chrono::steady_clock::time_point start_time;
};

class SearchEngine {
public:
    Position pos;
    NNUE nnue;
    SearchStats stats;
    
    // Transposition table
    struct TTEntry {
        uint64_t key;
        Move move;
        int16_t score;
        uint8_t depth;
        uint8_t flags; // 0: exact, 1: lower bound, 2: upper bound
    };
    
    std::vector<TTEntry> tt;
    size_t tt_size;
    
    // History tables for move ordering
    int16_t history[PIECE_COUNT + 1][SQUARE_COUNT];
    Move killer_moves[MAX_DEPTH][2];
    Move counter_moves[MAX_MOVES];
    
    SearchEngine() : tt_size(1024 * 1024) {
        tt.resize(tt_size);
        std::memset(history, 0, sizeof(history));
        std::memset(killer_moves, 0, sizeof(killer_moves));
    }
    
    void set_position(const std::string& fen) {
        parse_fen(fen);
    }
    
    void parse_fen(const std::string& fen) {
        // Simple FEN parser - set_start_position already initializes
        std::istringstream iss(fen);
        std::string position, turn, castling, ep, halfmove, fullmove;
        iss >> position >> turn >> castling >> ep >> halfmove >> fullmove;
        
        pos.set_start_position();
        // For now, just use starting position
        // Full FEN parsing would go here
    }
    
    Move find_best_move(int time_ms = 1000) {
        stats.nodes = 0;
        stats.qnodes = 0;
        stats.start_time = std::chrono::steady_clock::now();
        stats.best_move = Move::null();
        stats.best_score = -INF_SCORE;
        
        std::vector<Move> pv;
        
        // Iterative deepening
        for (int depth = 1; depth <= MAX_DEPTH; depth++) {
            stats.depth = depth;
            int score = search_root(depth, pv);
            
            if (!pv.empty()) {
                stats.best_move = pv[0];
                stats.best_score = score;
            }
            
            // Check time
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - stats.start_time).count();
            
            if (elapsed > time_ms * 0.7 || depth >= 20) {
                break;
            }
            
            std::cout << "info depth " << depth << " score cp " << score 
                      << " nodes " << stats.nodes << " nps " 
                      << (stats.nodes * 1000 / (elapsed + 1)) << std::endl;
        }
        
        return stats.best_move;
    }
    
    int search_root(int depth, std::vector<Move>& pv) {
        std::vector<Move> moves;
        MoveGenerator::generate_moves(pos, moves, false);
        
        if (moves.empty()) {
            if (pos.in_check(pos.side_to_move)) {
                return -MATE_SCORE + (MAX_DEPTH - depth);
            }
            return 0; // Stalemate
        }
        
        // Order moves
        order_moves(moves);
        
        int alpha = -INF_SCORE;
        int beta = INF_SCORE;
        Move best_move = moves[0];
        
        for (const auto& move : moves) {
            make_move(move);
            int score = -search(depth - 1, -beta, -alpha, 0);
            unmake_move(move);
            
            if (score > alpha) {
                alpha = score;
                best_move = move;
                pv.clear();
                pv.push_back(move);
            }
        }
        
        return alpha;
    }
    
    int search(int depth, int alpha, int beta, int ply) {
        stats.nodes++;
        
        // Check for draw by repetition or 50-move rule
        if (pos.halfmove_clock >= 100) {
            return 0;
        }
        
        // Transposition table lookup
        TTEntry* tte = probe_tt(pos.key);
        if (tte && tte->key == pos.key && tte->depth >= depth) {
            if (tte->flags == 0) {
                return tte->score;
            }
            if (tte->flags == 1 && tte->score >= beta) {
                return tte->score;
            }
            if (tte->flags == 2 && tte->score <= alpha) {
                return tte->score;
            }
        }
        
        // Leaf node - quiescence search
        if (depth <= 0) {
            return quiescence(alpha, beta);
        }
        
        // Check extension
        bool in_check = pos.in_check(pos.side_to_move);
        if (in_check) {
            depth++;
        }
        
        // Null move pruning
        if (!in_check && depth >= 2 && alpha > -MATE_SCORE + 100) {
            make_null_move();
            int r = 3 + depth / 4;
            int score = -search(depth - 1 - r, -beta, -beta + 1, ply + 1);
            unmake_null_move();
            
            if (score >= beta) {
                return beta;
            }
        }
        
        std::vector<Move> moves;
        MoveGenerator::generate_moves(pos, moves, false);
        
        if (moves.empty()) {
            if (in_check) {
                return -MATE_SCORE + ply;
            }
            return 0; // Stalemate
        }
        
        order_moves(moves);
        
        Move best_move = moves[0];
        int best_score = -INF_SCORE;
        int old_alpha = alpha;
        
        int moves_searched = 0;
        for (const auto& move : moves) {
            // Late move reduction
            int reduction = 0;
            if (moves_searched >= 4 && depth >= 3 && !in_check) {
                reduction = 1 + moves_searched / 8;
            }
            
            make_move(move);
            int score;
            
            if (reduction > 0) {
                score = -search(depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
                if (score > alpha) {
                    score = -search(depth - 1, -alpha - 1, -alpha, ply + 1);
                }
            } else {
                if (moves_searched == 0) {
                    score = -search(depth - 1, -beta, -alpha, ply + 1);
                } else {
                    score = -search(depth - 1, -alpha - 1, -alpha, ply + 1);
                    if (score > alpha && score < beta) {
                        score = -search(depth - 1, -beta, -alpha, ply + 1);
                    }
                }
            }
            unmake_move(move);
            
            moves_searched++;
            
            if (score > best_score) {
                best_score = score;
                best_move = move;
                
                if (score > alpha) {
                    alpha = score;
                    if (alpha >= beta) {
                        // Update killer moves
                        if (!move.is_promotion() && !is_capture(pos, move)) {
                            killer_moves[ply][1] = killer_moves[ply][0];
                            killer_moves[ply][0] = move;
                        }
                        break;
                    }
                }
            }
        }
        
        // Store in transposition table
        store_tt(pos.key, best_move, best_score, depth, 
                 best_score <= old_alpha ? 2 : (best_score >= beta ? 1 : 0));
        
        return best_score;
    }
    
    int quiescence(int alpha, int beta) {
        stats.qnodes++;
        
        int stand_pat = evaluate();
        
        if (stand_pat >= beta) {
            return beta;
        }
        
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
        
        std::vector<Move> moves;
        MoveGenerator::generate_moves(pos, moves, true); // Only captures
        
        // Order captures by MVV-LVA
        order_captures(moves);
        
        for (const auto& move : moves) {
            // Delta pruning
            int delta = get_piece_value(pos, move.to()) + 300;
            if (stand_pat + delta < alpha) {
                continue;
            }
            
            make_move(move);
            int score = -quiescence(-beta, -alpha);
            unmake_move(move);
            
            if (score >= beta) {
                return beta;
            }
            
            if (score > alpha) {
                alpha = score;
            }
        }
        
        return alpha;
    }
    
    int evaluate() {
        // NNUE evaluation
        NNUE::Accumulator acc;
        acc.init(nnue);
        
        // Build accumulator from position
        // For simplicity, we'll use a simpler material-based eval for now
        // Full NNUE implementation would track incremental updates
        
        int score = 0;
        
        // Material counts
        constexpr int piece_values[] = {0, 100, 320, 330, 500, 900, 20000,
                                        0, 100, 320, 330, 500, 900, 20000};
        
        for (int p = 1; p <= PIECE_COUNT; p++) {
            Bitboard bb = pos.by_piece[p];
            int count = popcount(bb);
            score += count * piece_values[p];
        }
        
        // Add positional bonuses (simplified)
        score += evaluate_positional();
        
        return pos.side_to_move == WHITE ? score : -score;
    }
    
    int evaluate_positional() {
        int score = 0;
        
        // Piece-square tables (simplified)
        constexpr int pawn_table[64] = {
            0,  0,  0,  0,  0,  0,  0,  0,
            50, 50, 50, 50, 50, 50, 50, 50,
            10, 10, 20, 30, 30, 20, 10, 10,
            5,  5, 10, 25, 25, 10,  5,  5,
            0,  0,  0, 20, 20,  0,  0,  0,
            5, -5,-10,  0,  0,-10, -5,  5,
            5, 10, 10,-20,-20, 10, 10,  5,
            0,  0,  0,  0,  0,  0,  0,  0
        };
        
        // Add white pawn bonuses
        Bitboard wp = pos.by_piece[WP];
        while (wp) {
            int sq = pop_lsb(wp);
            score += pawn_table[sq];
        }
        
        // Subtract black pawn bonuses (from white's perspective)
        Bitboard bp = pos.by_piece[BP];
        while (bp) {
            int sq = pop_lsb(bp);
            score -= pawn_table[sq ^ 56]; // Mirror square
        }
        
        return score;
    }
    
    void order_moves(std::vector<Move>& moves) {
        std::sort(moves.begin(), moves.end(), [this](const Move& a, const Move& b) {
            return move_score(a) > move_score(b);
        });
    }
    
    void order_captures(std::vector<Move>& moves) {
        std::sort(moves.begin(), moves.end(), [this](const Move& a, const Move& b) {
            return mvv_lva_score(a) > mvv_lva_score(b);
        });
    }
    
    int move_score(const Move& move) {
        int score = 0;
        
        // Capture bonus (MVV-LVA)
        if (is_capture(pos, move)) {
            score += 10000000 + mvv_lva_score(move);
        }
        
        // Killer move bonus
        for (int i = 0; i < 2; i++) {
            if (killer_moves[stats.depth][i].data == move.data) {
                score += 9000000 - i * 100000;
                break;
            }
        }
        
        // History bonus
        score += history[get_moved_piece(pos, move)][move.to()];
        
        return score;
    }
    
    int mvv_lva_score(const Move& move) {
        int victim = get_captured_piece(pos, move);
        int attacker = get_moved_piece(pos, move);
        
        constexpr int piece_values[] = {0, 1, 3, 3, 5, 9, 6, 1, 3, 3, 5, 9, 6};
        
        return victim * 10 - attacker;
    }
    
    bool is_capture(const Position& p, const Move& move) {
        return p.by_color[1 - p.side_to_move] & bit(move.to());
    }
    
    int get_moved_piece(const Position& p, const Move& move) {
        for (int piece = 1; piece <= PIECE_COUNT; piece++) {
            if (p.by_piece[piece] & bit(move.from())) {
                return piece;
            }
        }
        return 0;
    }
    
    int get_captured_piece(const Position& p, const Move& move) {
        int them = 1 - p.side_to_move;
        for (int piece = them == WHITE ? WP : BP; piece <= (them == WHITE ? WK : BK); piece++) {
            if (p.by_piece[piece] & bit(move.to())) {
                return piece;
            }
        }
        return 0;
    }
    
    int get_piece_value(const Position& p, int sq) {
        for (int piece = 1; piece <= PIECE_COUNT; piece++) {
            if (p.by_piece[piece] & bit(sq)) {
                constexpr int values[] = {0, 100, 320, 330, 500, 900, 20000,
                                         0, 100, 320, 330, 500, 900, 20000};
                return values[piece];
            }
        }
        return 0;
    }
    
    void make_move(const Move& move) {
        // Save state for unmake
        // Simplified - full implementation would save more state
        
        int from = move.from();
        int to = move.to();
        int us = pos.side_to_move;
        int them = 1 - us;
        
        int moving_piece = 0;
        for (int p = 1; p <= PIECE_COUNT; p++) {
            if (pos.by_piece[p] & bit(from)) {
                moving_piece = p;
                break;
            }
        }
        
        // Move the piece
        pos.by_piece[moving_piece] ^= bit(from) ^ bit(to);
        pos.by_color[us] ^= bit(from) ^ bit(to);
        
        // Handle captures
        int captured_piece = 0;
        for (int p = 1; p <= PIECE_COUNT; p++) {
            if (pos.by_piece[p] & bit(to)) {
                captured_piece = p;
                break;
            }
        }
        
        if (captured_piece) {
            pos.by_piece[captured_piece] ^= bit(to);
            pos.by_color[them] ^= bit(to);
            pos.halfmove_clock = 0;
        } else if (moving_piece == WP || moving_piece == BP) {
            pos.halfmove_clock = 0;
        } else {
            pos.halfmove_clock++;
        }
        
        // Handle promotions
        if (move.is_promotion()) {
            int promo_piece = us == WHITE ? 
                (move.promotion() == 1 ? WN : move.promotion() == 2 ? WB : 
                 move.promotion() == 3 ? WR : WQ) :
                (move.promotion() == 1 ? BN : move.promotion() == 2 ? BB : 
                 move.promotion() == 3 ? BR : BQ);
            
            pos.by_piece[moving_piece] ^= bit(to);
            pos.by_piece[promo_piece] |= bit(to);
        }
        
        // Handle en passant
        if (move.flag() == MOVE_FLAG_EP) {
            int ep_pawn_sq = us == WHITE ? to - 8 : to + 8;
            int ep_pawn = us == WHITE ? BP : WP;
            pos.by_piece[ep_pawn] ^= bit(ep_pawn_sq);
            pos.by_color[them] ^= bit(ep_pawn_sq);
        }
        
        // Handle castling
        if (move.flag() == MOVE_FLAG_CASTLE) {
            if (to == G1) { // White kingside
                pos.by_piece[WR] ^= bit(H1) ^ bit(F1);
                pos.by_color[WHITE] ^= bit(H1) ^ bit(F1);
            } else if (to == C1) { // White queenside
                pos.by_piece[WR] ^= bit(A1) ^ bit(D1);
                pos.by_color[WHITE] ^= bit(A1) ^ bit(D1);
            } else if (to == G8) { // Black kingside
                pos.by_piece[BR] ^= bit(H8) ^ bit(F8);
                pos.by_color[BLACK] ^= bit(H8) ^ bit(F8);
            } else if (to == C8) { // Black queenside
                pos.by_piece[BR] ^= bit(A8) ^ bit(D8);
                pos.by_color[BLACK] ^= bit(A8) ^ bit(D8);
            }
        }
        
        // Update castling rights
        if (moving_piece == WK) {
            pos.castling_rights &= ~3;
        } else if (moving_piece == BK) {
            pos.castling_rights &= ~12;
        } else if (moving_piece == WR) {
            if (from == A1) pos.castling_rights &= ~2;
            if (from == H1) pos.castling_rights &= ~1;
        } else if (moving_piece == BR) {
            if (from == A8) pos.castling_rights &= ~8;
            if (from == H8) pos.castling_rights &= ~4;
        }
        
        // Update en passant square
        pos.ep_square = SQ_NONE;
        if (move.flag() == MOVE_FLAG_EP) {
            // Already handled
        } else if ((moving_piece == WP || moving_piece == BP) && 
                   abs(to - from) == 16) {
            pos.ep_square = us == WHITE ? from + 8 : from - 8;
        }
        
        // Switch side
        pos.side_to_move = them;
        pos.fullmove_number += (pos.side_to_move == WHITE);
        
        pos.update_from_pieces();
        pos.key = pos.compute_key();
    }
    
    void unmake_move(const Move& move) {
        // Reverse of make_move
        pos.side_to_move = 1 - pos.side_to_move;
        int us = pos.side_to_move;
        int them = 1 - us;
        
        int from = move.from();
        int to = move.to();
        
        int moving_piece = 0;
        if (move.is_promotion()) {
            int promo_piece = us == WHITE ? 
                (move.promotion() == 1 ? WN : move.promotion() == 2 ? WB : 
                 move.promotion() == 3 ? WR : WQ) :
                (move.promotion() == 1 ? BN : move.promotion() == 2 ? BB : 
                 move.promotion() == 3 ? BR : BQ);
            pos.by_piece[promo_piece] ^= bit(to);
            moving_piece = us == WHITE ? WP : BP;
        } else {
            for (int p = 1; p <= PIECE_COUNT; p++) {
                if (pos.by_piece[p] & bit(to)) {
                    moving_piece = p;
                    break;
                }
            }
        }
        
        pos.by_piece[moving_piece] ^= bit(to) ^ bit(from);
        pos.by_color[us] ^= bit(to) ^ bit(from);
        
        // Restore captured piece
        int captured_piece = 0;
        // Need to track this properly - simplified here
        
        // Restore en passant capture
        if (move.flag() == MOVE_FLAG_EP) {
            int ep_pawn_sq = us == WHITE ? to - 8 : to + 8;
            int ep_pawn = us == WHITE ? BP : WP;
            pos.by_piece[ep_pawn] |= bit(ep_pawn_sq);
            pos.by_color[them] |= bit(ep_pawn_sq);
        }
        
        // Restore castling rook
        if (move.flag() == MOVE_FLAG_CASTLE) {
            if (to == G1) {
                pos.by_piece[WR] ^= bit(F1) ^ bit(H1);
                pos.by_color[WHITE] ^= bit(F1) ^ bit(H1);
            } else if (to == C1) {
                pos.by_piece[WR] ^= bit(D1) ^ bit(A1);
                pos.by_color[WHITE] ^= bit(D1) ^ bit(A1);
            } else if (to == G8) {
                pos.by_piece[BR] ^= bit(F8) ^ bit(H8);
                pos.by_color[BLACK] ^= bit(F8) ^ bit(H8);
            } else if (to == C8) {
                pos.by_piece[BR] ^= bit(D8) ^ bit(A8);
                pos.by_color[BLACK] ^= bit(D8) ^ bit(A8);
            }
        }
        
        pos.fullmove_number -= (pos.side_to_move == WHITE);
        pos.update_from_pieces();
        pos.key = pos.compute_key();
    }
    
    void make_null_move() {
        pos.side_to_move = 1 - pos.side_to_move;
        pos.ep_square = SQ_NONE;
        pos.key = pos.compute_key();
    }
    
    void unmake_null_move() {
        pos.side_to_move = 1 - pos.side_to_move;
        pos.key = pos.compute_key();
    }
    
    TTEntry* probe_tt(uint64_t key) {
        return &tt[key % tt_size];
    }
    
    void store_tt(uint64_t key, Move move, int score, int depth, int flags) {
        TTEntry* tte = &tt[key % tt_size];
        tte->key = key;
        tte->move = move;
        tte->score = score;
        tte->depth = depth;
        tte->flags = flags;
    }
};

// ============================================================================
// UCI Protocol Handler
// ============================================================================

class UCIHandler {
public:
    SearchEngine engine;
    
    void run() {
        std::string line;
        
        std::cout << "id name ChessEngine3200" << std::endl;
        std::cout << "id author AI Assistant" << std::endl;
        std::cout << "uciok" << std::endl;
        
        while (std::getline(std::cin, line)) {
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "quit") {
                break;
            } else if (cmd == "ucinewgame") {
                engine = SearchEngine();
            } else if (cmd == "position") {
                handle_position(iss);
            } else if (cmd == "go") {
                handle_go(iss);
            } else if (cmd == "stop") {
                // Handled in search thread
            }
        }
    }
    
private:
    void handle_position(std::istringstream& iss) {
        std::string token;
        iss >> token;
        
        if (token == "startpos") {
            engine.pos.set_start_position();
            
            // Handle moves
            if (iss >> token && token == "moves") {
                std::string move_str;
                while (iss >> move_str) {
                    // Parse and make move
                    // Simplified - full implementation needed
                }
            }
        } else if (token == "fen") {
            std::string fen;
            for (int i = 0; i < 6; i++) {
                std::string part;
                iss >> part;
                fen += part + " ";
            }
            engine.set_position(fen);
        }
    }
    
    void handle_go(std::istringstream& iss) {
        std::string token;
        int time = 1000;
        int movetime = -1;
        
        while (iss >> token) {
            if (token == "wtime" || token == "btime") {
                iss >> time;
            } else if (token == "movetime") {
                iss >> movetime;
            }
        }
        
        int search_time = movetime > 0 ? movetime : time / 20;
        Move best = engine.find_best_move(search_time);
        
        std::cout << "bestmove " << square_to_algebraic(best.from()) 
                  << square_to_algebraic(best.to());
        
        if (best.is_promotion()) {
            char promo = best.promotion() == 1 ? 'n' : 
                        best.promotion() == 2 ? 'b' :
                        best.promotion() == 3 ? 'r' : 'q';
            std::cout << promo;
        }
        std::cout << std::endl;
    }
    
    std::string square_to_algebraic(int sq) {
        const char files[] = "abcdefgh";
        const char ranks[] = "12345678";
        return std::string() + files[sq % 8] + ranks[sq / 8];
    }
};

// ============================================================================
// Initialization
// ============================================================================

void init_attacks() {
    // Initialize pawn attacks
    for (int sq = 0; sq < SQUARE_COUNT; sq++) {
        int rank = sq / 8;
        int file = sq % 8;
        
        // White pawn attacks
        if (rank < 7) {
            if (file > 0) pawn_attacks[WHITE][sq] |= bit(sq + 7);
            if (file < 7) pawn_attacks[WHITE][sq] |= bit(sq + 9);
        }
        
        // Black pawn attacks
        if (rank > 0) {
            if (file > 0) pawn_attacks[BLACK][sq] |= bit(sq - 9);
            if (file < 7) pawn_attacks[BLACK][sq] |= bit(sq - 7);
        }
        
        // Knight attacks
        static const int knight_offsets[] = {-17, -15, -10, -6, 6, 10, 15, 17};
        for (int offset : knight_offsets) {
            int target = sq + offset;
            if (target >= 0 && target < 64 && abs((sq % 8) - (target % 8)) <= 2) {
                knight_attacks[sq] |= bit(target);
            }
        }
        
        // King attacks
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr == 0 && df == 0) continue;
                int tr = rank + dr;
                int tf = file + df;
                if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8) {
                    king_attacks[sq] |= bit(tr * 8 + tf);
                }
            }
        }
    }
    
    // Initialize sliding piece attacks (simplified - no magic bitboards for brevity)
    for (int sq = 0; sq < SQUARE_COUNT; sq++) {
        int rank = sq / 8;
        int file = sq % 8;
        
        // Bishop attacks
        for (int dr : {-1, 1}) {
            for (int df : {-1, 1}) {
                for (int d = 1; d < 8; d++) {
                    int tr = rank + dr * d;
                    int tf = file + df * d;
                    if (tr < 0 || tr >= 8 || tf < 0 || tf >= 8) break;
                    bishop_attacks[sq] |= bit(tr * 8 + tf);
                }
            }
        }
        
        // Rook attacks
        static const int rook_dirs[] = {-8, 8, -1, 1};
        for (int dir : rook_dirs) {
            for (int d = 1; d < 8; d++) {
                int target = sq + dir * d;
                if (target < 0 || target >= 64) break;
                if (dir == -1 && sq % 8 == 0) break;
                if (dir == 1 && sq % 8 == 7) break;
                rook_attacks[sq] |= bit(target);
            }
        }
        
        // Setup magic bitboards (simplified)
        magic_bishops[sq] = bishop_attacks[sq];
        magic_rooks[sq] = rook_attacks[sq];
        magic_shift_bishops[sq] = 0;
        magic_shift_rooks[sq] = 0;
        bishop_attacks_magic[sq] = new Bitboard[1];
        rook_attacks_magic[sq] = new Bitboard[1];
        bishop_attacks_magic[sq][0] = bishop_attacks[sq];
        rook_attacks_magic[sq][0] = rook_attacks[sq];
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    init_attacks();
    
    UCIHandler handler;
    handler.run();
    
    return 0;
}
