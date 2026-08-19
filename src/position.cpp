#include "position.h"
#include "evaluation.h"
#include <sstream>
#include <cstring>
#include <cctype>

// Zobrist keys for hashing
static uint64_t zobrist_pieces[12][64];
static uint64_t zobrist_castling[16];
static uint64_t zobrist_ep[8];
static uint64_t zobrist_side;

static void init_zobrist() {
    // Simple PRNG for zobrist keys
    uint64_t seed = 0x123456789ABCDEF0ULL;
    
    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            zobrist_pieces[p][s] = seed;
        }
    
    for (int c = 0; c < 16; ++c) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        zobrist_castling[c] = seed;
    }
    
    for (int f = 0; f < 8; ++f) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        zobrist_ep[f] = seed;
    }
    
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    zobrist_side = seed;
}

Position::Position() {
    static bool initialized = false;
    if (!initialized) {
        init_bitboards();
        init_zobrist();
        initialized = true;
    }
    set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Position::update_key() {
    key_ = 0;
    for (int sq = 0; sq < 64; ++sq) {
        if (board_[sq] != NO_PIECE) {
            key_ ^= zobrist_pieces[board_[sq] - 1][sq];
        }
    }
    key_ ^= zobrist_castling[castling_];
    if (ep_square_ != SQ_NONE)
        key_ ^= zobrist_ep[square_file(ep_square_)];
    if (side_to_move_ == BLACK)
        key_ ^= zobrist_side;
}

void Position::update_check_info() {
    Color us = side_to_move_;
    Color them = !us;
    
    Square ksq = SQ_NONE;
    for (int sq = 0; sq < 64; ++sq)
        if (board_[sq] == make_piece(us, KING)) {
            ksq = static_cast<Square>(sq);
            break;
        }
    
    checkers_ = 0;
    slider_attackers_ = 0;
    
    // Pawn checks
    Bitboard pawn_attackers = PAWN_ATTACKS[them][ksq] & occupied(them, PAWN);
    if (pawn_attackers) checkers_ |= pawn_attackers;
    
    // Knight checks
    Bitboard knight_attackers = KNIGHT_ATTACKS[ksq] & occupied(them, KNIGHT);
    if (knight_attackers) checkers_ |= knight_attackers;
    
    // King checks (shouldn't happen but for completeness)
    Bitboard king_attackers = KING_ATTACKS[ksq] & occupied(them, KING);
    if (king_attackers) checkers_ |= king_attackers;
    
    // Slider checks (bishop/queen diagonals) - simplified
    File kf = square_file(ksq);
    Rank kr = square_rank(ksq);
    
    // Check diagonal attacks
    const int diag_dirs[4][2] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
    for (int d = 0; d < 4; ++d) {
        for (int dist = 1; dist < 8; ++dist) {
            int nf = kf + diag_dirs[d][0] * dist;
            int nr = kr + diag_dirs[d][1] * dist;
            if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
            Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(nr));
            Piece p = board_[sq];
            if (p != NO_PIECE) {
                if (color_of(p) == them && (type_of(p) == BISHOP || type_of(p) == QUEEN)) {
                    checkers_ |= (1ULL << sq);
                    slider_attackers_ |= (1ULL << sq);
                }
                break;
            }
        }
    }
    
    // Check orthogonal attacks
    const int ortho_dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    for (int d = 0; d < 4; ++d) {
        for (int dist = 1; dist < 8; ++dist) {
            int nf = kf + ortho_dirs[d][0] * dist;
            int nr = kr + ortho_dirs[d][1] * dist;
            if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
            Square sq = make_square(static_cast<File>(nf), static_cast<Rank>(nr));
            Piece p = board_[sq];
            if (p != NO_PIECE) {
                if (color_of(p) == them && (type_of(p) == ROOK || type_of(p) == QUEEN)) {
                    checkers_ |= (1ULL << sq);
                    slider_attackers_ |= (1ULL << sq);
                }
                break;
            }
        }
    }
}

void Position::update_pin_info() {
    pinned_ = 0;
    // Simplified pin detection - full implementation would be more complex
}

bool Position::is_attacked(Square s, Color by) const {
    return is_attacked_by_pawn(s, by) ||
           is_attacked_by_knight(s, by) ||
           is_attacked_by_king(s, by) ||
           is_attacked_by_slider(s, by);
}

bool Position::is_attacked_by_pawn(Square s, Color by) const {
    return PAWN_ATTACKS[by][s] & occupied(by, PAWN);
}

bool Position::is_attacked_by_knight(Square s, Color by) const {
    return KNIGHT_ATTACKS[s] & occupied(by, KNIGHT);
}

bool Position::is_attacked_by_king(Square s, Color by) const {
    return KING_ATTACKS[s] & occupied(by, KING);
}

bool Position::is_attacked_by_slider(Square s, Color by) const {
    File f = square_file(s);
    Rank r = square_rank(s);
    
    // Diagonal attacks (bishop/queen)
    Bitboard bishops = occupied(by, BISHOP) | occupied(by, QUEEN);
    if (bishops) {
        // Simplified - would need proper magic bitboards
        for (int sq = 0; sq < 64; ++sq) {
            if (bishops & (1ULL << sq)) {
                Square bsq = static_cast<Square>(sq);
                int bf = square_file(bsq), br = square_rank(bsq);
                if (abs(bf - f) == abs(br - r)) {
                    // Check if path is clear
                    bool blocked = false;
                    int df = (f > bf) ? 1 : -1;
                    int dr = (r > br) ? 1 : -1;
                    Square csq = bsq;
                    while ((csq = make_square(static_cast<File>(square_file(csq) + df),
                                              static_cast<Rank>(square_rank(csq) + dr))) != s) {
                        if (occupied() & (1ULL << csq)) {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) return true;
                }
            }
        }
    }
    
    // Orthogonal attacks (rook/queen)
    Bitboard rooks = occupied(by, ROOK) | occupied(by, QUEEN);
    if (rooks) {
        for (int sq = 0; sq < 64; ++sq) {
            if (rooks & (1ULL << sq)) {
                Square rsq = static_cast<Square>(sq);
                int rf = square_file(rsq), rr = square_rank(rsq);
                if (rf == f || rr == r) {
                    bool blocked = false;
                    int df = (f > rf) ? 1 : (f < rf) ? -1 : 0;
                    int dr = (r > rr) ? 1 : (r < rr) ? -1 : 0;
                    Square csq = rsq;
                    while ((csq = make_square(static_cast<File>(square_file(csq) + df),
                                              static_cast<Rank>(square_rank(csq) + dr))) != s) {
                        if (occupied() & (1ULL << csq)) {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) return true;
                }
            }
        }
    }
    
    return false;
}

void Position::set_fen(const std::string& fen) {
    std::istringstream iss(fen);
    std::string board_part, color_part, castling_part, ep_part;
    int halfmove = 0, fullmove = 1;
    
    iss >> board_part >> color_part >> castling_part >> ep_part >> halfmove >> fullmove;
    
    // Clear board
    memset(board_, NO_PIECE, sizeof(board_));
    occupied_ = 0;
    memset(by_color_, 0, sizeof(by_color_));
    memset(by_type_, 0, sizeof(by_type_));
    
    // Parse board
    Square sq = A8;
    for (char c : board_part) {
        if (c == '/') continue;
        if (isdigit(c)) {
            sq = static_cast<Square>(square_index(sq) + (c - '0'));
        } else {
            Piece p = NO_PIECE;
            Color color = isupper(c) ? WHITE : BLACK;
            char pc = tolower(c);
            
            switch (pc) {
                case 'p': p = make_piece(color, PAWN); break;
                case 'n': p = make_piece(color, KNIGHT); break;
                case 'b': p = make_piece(color, BISHOP); break;
                case 'r': p = make_piece(color, ROOK); break;
                case 'q': p = make_piece(color, QUEEN); break;
                case 'k': p = make_piece(color, KING); break;
            }
            
            if (p != NO_PIECE) {
                board_[sq] = p;
                occupied_ |= (1ULL << sq);
                by_color_[color] |= (1ULL << sq);
                by_type_[type_of(p)] |= (1ULL << sq);
            }
            sq = static_cast<Square>(square_index(sq) + 1);
        }
    }
    
    // Side to move
    side_to_move_ = (color_part == "w") ? WHITE : BLACK;
    
    // Castling rights
    castling_ = NO_CASTLING;
    for (char c : castling_part) {
        if (c == 'K') castling_ |= WHITE_OO;
        else if (c == 'Q') castling_ |= WHITE_OOO;
        else if (c == 'k') castling_ |= BLACK_OO;
        else if (c == 'q') castling_ |= BLACK_OOO;
    }
    
    // En passant
    ep_square_ = SQ_NONE;
    if (ep_part != "-") {
        ep_square_ = make_square(static_cast<File>(ep_part[0] - 'a'),
                                  static_cast<Rank>(ep_part[1] - '1'));
    }
    
    halfmove_clock_ = static_cast<uint8_t>(halfmove);
    fullmove_number_ = static_cast<uint8_t>(fullmove);
    
    update_key();
    update_check_info();
    update_pin_info();
}

std::string Position::fen() const {
    std::string result;
    
    // Board
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            Piece p = board_[sq];
            
            if (p == NO_PIECE) {
                empty++;
            } else {
                if (empty > 0) result += std::to_string(empty);
                empty = 0;
                
                char c;
                switch (type_of(p)) {
                    case PAWN: c = 'p'; break;
                    case KNIGHT: c = 'n'; break;
                    case BISHOP: c = 'b'; break;
                    case ROOK: c = 'r'; break;
                    case QUEEN: c = 'q'; break;
                    case KING: c = 'k'; break;
                    default: c = '?';
                }
                if (color_of(p) == WHITE) c = toupper(c);
                result += c;
            }
        }
        if (empty > 0) result += std::to_string(empty);
        if (r > 0) result += '/';
    }
    
    // Side to move
    result += ' ';
    result += (side_to_move_ == WHITE) ? 'w' : 'b';
    
    // Castling
    result += ' ';
    if (castling_ == NO_CASTLING) {
        result += '-';
    } else {
        if (castling_ & WHITE_OO) result += 'K';
        if (castling_ & WHITE_OOO) result += 'Q';
        if (castling_ & BLACK_OO) result += 'k';
        if (castling_ & BLACK_OOO) result += 'q';
    }
    
    // En passant
    result += ' ';
    if (ep_square_ == SQ_NONE) {
        result += '-';
    } else {
        result += square_to_string(ep_square_);
    }
    
    // Halfmove and fullmove
    result += ' ' + std::to_string(halfmove_clock_);
    result += ' ' + std::to_string(fullmove_number_);
    
    return result;
}

void Position::save_state(State& s) const {
    s.occupied = occupied_;
    memcpy(s.by_color, by_color_, sizeof(by_color_));
    memcpy(s.by_type, by_type_, sizeof(by_type_));
    memcpy(s.board, board_, sizeof(board_));
    s.castling = castling_;
    s.ep_square = ep_square_;
    s.halfmove_clock = halfmove_clock_;
    s.fullmove_number = fullmove_number_;
    s.side_to_move = side_to_move_;
    s.checkers = checkers_;
    s.pinned = pinned_;
}

void Position::make_move(Move m) {
    if (m.is_null()) return;
    
    Square from = m.from_sq();
    Square to = m.to_sq();
    Piece moving = board_[from];
    Piece captured = board_[to];
    MoveType mt = m.type();
    
    // Update hash
    key_ ^= zobrist_pieces[moving - 1][from];
    
    // Handle captures
    if (captured != NO_PIECE || mt == MOVE_EN_PASSANT) {
        if (captured == NO_PIECE) {
            // En passant capture
            Square ep_capture = make_square(square_file(to), square_rank(from));
            captured = board_[ep_capture];
            key_ ^= zobrist_pieces[captured - 1][ep_capture];
            board_[ep_capture] = NO_PIECE;
            by_color_[color_of(captured)] &= ~(1ULL << ep_capture);
            by_type_[type_of(captured)] &= ~(1ULL << ep_capture);
            occupied_ &= ~(1ULL << ep_capture);
        } else {
            key_ ^= zobrist_pieces[captured - 1][to];
            by_color_[color_of(captured)] &= ~(1ULL << to);
            by_type_[type_of(captured)] &= ~(1ULL << to);
            occupied_ &= ~(1ULL << to);
        }
        halfmove_clock_ = 0;
    } else if (type_of(moving) == PAWN) {
        halfmove_clock_ = 0;
    } else {
        halfmove_clock_++;
    }
    
    // Move piece
    board_[from] = NO_PIECE;
    board_[to] = moving;
    by_color_[color_of(moving)] = (by_color_[color_of(moving)] & ~(1ULL << from)) | (1ULL << to);
    by_type_[type_of(moving)] = (by_type_[type_of(moving)] & ~(1ULL << from)) | (1ULL << to);
    occupied_ = (occupied_ & ~(1ULL << from)) | (1ULL << to);
    
    // Handle promotions
    if (mt == MOVE_PROMOTION) {
        PieceType pt = m.promotion_type();
        Piece promoted = make_piece(side_to_move_, pt);
        board_[to] = promoted;
        by_type_[PAWN] &= ~(1ULL << to);
        by_type_[pt] |= (1ULL << to);
        key_ ^= zobrist_pieces[moving - 1][to];
        key_ ^= zobrist_pieces[promoted - 1][to];
    }
    
    // Handle castling
    if (mt == MOVE_CASTLING) {
        if (to == G1) { // White kingside
            Piece rook = board_[H1];
            board_[H1] = NO_PIECE;
            board_[F1] = rook;
            by_color_[WHITE] = (by_color_[WHITE] & ~(1ULL << H1)) | (1ULL << F1);
            by_type_[ROOK] = (by_type_[ROOK] & ~(1ULL << H1)) | (1ULL << F1);
            occupied_ = (occupied_ & ~(1ULL << H1)) | (1ULL << F1);
            key_ ^= zobrist_pieces[W_ROOK - 1][H1];
            key_ ^= zobrist_pieces[W_ROOK - 1][F1];
        } else if (to == C1) { // White queenside
            Piece rook = board_[A1];
            board_[A1] = NO_PIECE;
            board_[D1] = rook;
            by_color_[WHITE] = (by_color_[WHITE] & ~(1ULL << A1)) | (1ULL << D1);
            by_type_[ROOK] = (by_type_[ROOK] & ~(1ULL << A1)) | (1ULL << D1);
            occupied_ = (occupied_ & ~(1ULL << A1)) | (1ULL << D1);
            key_ ^= zobrist_pieces[W_ROOK - 1][A1];
            key_ ^= zobrist_pieces[W_ROOK - 1][D1];
        } else if (to == G8) { // Black kingside
            Piece rook = board_[H8];
            board_[H8] = NO_PIECE;
            board_[F8] = rook;
            by_color_[BLACK] = (by_color_[BLACK] & ~(1ULL << H8)) | (1ULL << F8);
            by_type_[ROOK] = (by_type_[ROOK] & ~(1ULL << H8)) | (1ULL << F8);
            occupied_ = (occupied_ & ~(1ULL << H8)) | (1ULL << F8);
            key_ ^= zobrist_pieces[B_ROOK - 1][H8];
            key_ ^= zobrist_pieces[B_ROOK - 1][F8];
        } else if (to == C8) { // Black queenside
            Piece rook = board_[A8];
            board_[A8] = NO_PIECE;
            board_[D8] = rook;
            by_color_[BLACK] = (by_color_[BLACK] & ~(1ULL << A8)) | (1ULL << D8);
            by_type_[ROOK] = (by_type_[ROOK] & ~(1ULL << A8)) | (1ULL << D8);
            occupied_ = (occupied_ & ~(1ULL << A8)) | (1ULL << D8);
            key_ ^= zobrist_pieces[B_ROOK - 1][A8];
            key_ ^= zobrist_pieces[B_ROOK - 1][D8];
        }
    }
    
    // Update castling rights
    uint8_t old_castling = castling_;
    if (moving == W_KING) castling_ &= ~(WHITE_OO | WHITE_OOO);
    else if (moving == B_KING) castling_ &= ~(BLACK_OO | BLACK_OOO);
    else if (moving == W_ROOK) {
        if (from == A1) castling_ &= ~WHITE_OOO;
        else if (from == H1) castling_ &= ~WHITE_OO;
    } else if (moving == B_ROOK) {
        if (from == A8) castling_ &= ~BLACK_OOO;
        else if (from == H8) castling_ &= ~BLACK_OO;
    }
    if (old_castling != castling_) {
        key_ ^= zobrist_castling[old_castling];
        key_ ^= zobrist_castling[castling_];
    }
    
    // Update en passant
    Square old_ep = ep_square_;
    ep_square_ = SQ_NONE;
    if (type_of(moving) == PAWN && abs(square_rank(to) - square_rank(from)) == 2) {
        ep_square_ = make_square(square_file(from), static_cast<Rank>((square_rank(from) + square_rank(to)) / 2));
        key_ ^= zobrist_ep[square_file(ep_square_)];
    } else if (old_ep != SQ_NONE) {
        key_ ^= zobrist_ep[square_file(old_ep)];
    }
    
    // Switch side
    side_to_move_ = !side_to_move_;
    key_ ^= zobrist_side;
    
    // Update check info
    update_check_info();
    update_pin_info();
}

void Position::unmake_move(Move m, State& prev) {
    if (m.is_null()) return;
    
    occupied_ = prev.occupied;
    memcpy(by_color_, prev.by_color, sizeof(by_color_));
    memcpy(by_type_, prev.by_type, sizeof(by_type_));
    memcpy(board_, prev.board, sizeof(board_));
    castling_ = prev.castling;
    ep_square_ = prev.ep_square;
    halfmove_clock_ = prev.halfmove_clock;
    fullmove_number_ = prev.fullmove_number;
    side_to_move_ = prev.side_to_move;
    checkers_ = prev.checkers;
    pinned_ = prev.pinned;
    
    update_key();
}

int Position::see(Move m) const {
    // Simplified SEE - just returns material balance of capture
    Square to = m.to_sq();
    Piece captured = board_[to];
    Piece attacker = board_[m.from_sq()];
    
    if (captured == NO_PIECE && m.type() != MOVE_EN_PASSANT) return 0;
    
    Score gain = PIECE_VALUES[type_of(captured)];
    if (m.type() == MOVE_EN_PASSANT) gain = PIECE_VALUES[PAWN];
    gain -= PIECE_VALUES[type_of(attacker)];
    
    if (m.type() == MOVE_PROMOTION) {
        gain += PIECE_VALUES[m.promotion_type()] - PIECE_VALUES[PAWN];
    }
    
    return gain;
}

bool Position::is_valid() const {
    // Basic validation
    int white_kings = bitboard_count(occupied(WHITE, KING));
    int black_kings = bitboard_count(occupied(BLACK, KING));
    
    if (white_kings != 1 || black_kings != 1) return false;
    
    // Check no two pieces on same square
    if (bitboard_count(occupied_) != 
        bitboard_count(occupied(WHITE)) + bitboard_count(occupied(BLACK))) return false;
    
    return true;
}
