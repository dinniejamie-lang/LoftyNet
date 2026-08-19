#ifndef POSITION_H
#define POSITION_H

#include "types.h"
#include "bitboard.h"
#include "move.h"
#include <array>
#include <string>

// Castling rights
enum CastlingRights : uint8_t {
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    NO_CASTLING = 0
};

// Game state for undo
struct State {
    Bitboard occupied;
    Bitboard by_color[2];
    Bitboard by_type[6];
    Piece board[64];
    uint8_t castling;
    Square ep_square;
    uint8_t halfmove_clock;
    uint8_t fullmove_number;
    Color side_to_move;
    Bitboard checkers;
    Bitboard pinned;
};

class Position {
public:
    Position();
    
    // Board access
    Piece piece_at(Square s) const { return board_[s]; }
    Bitboard occupied() const { return occupied_; }
    Bitboard occupied(Color c) const { return by_color_[c]; }
    Bitboard occupied(PieceType pt) const { return by_type_[pt]; }
    Bitboard occupied(Color c, PieceType pt) const { return by_color_[c] & by_type_[pt]; }
    
    Color side_to_move() const { return side_to_move_; }
    Square ep_square() const { return ep_square_; }
    uint8_t castling_rights() const { return castling_; }
    uint8_t halfmove_clock() const { return halfmove_clock_; }
    uint8_t fullmove_number() const { return fullmove_number_; }
    
    // Check information
    bool in_check() const { return checkers_ != 0; }
    Bitboard checkers() const { return checkers_; }
    Bitboard pinned() const { return pinned_; }
    
    // FEN parsing
    void set_fen(const std::string& fen);
    std::string fen() const;
    
    // Make/unmake moves
    void make_move(Move m);
    void unmake_move(Move m, State& prev);
    
    // State saving
    void save_state(State& s) const;
    
    // Hash key (for transposition table)
    uint64_t key() const { return key_; }
    
    // Is square attacked
    bool is_attacked(Square s, Color by) const;
    bool is_attacked_by_king(Square s, Color by) const;
    bool is_attacked_by_knight(Square s, Color by) const;
    bool is_attacked_by_pawn(Square s, Color by) const;
    bool is_attacked_by_slider(Square s, Color by) const;
    
    // Static exchange evaluation
    int see(Move m) const;
    
    // Is position valid
    bool is_valid() const;
    
private:
    void update_key();
    void update_check_info();
    void update_pin_info();
    
    Bitboard occupied_;
    Bitboard by_color_[2];
    Bitboard by_type_[6];
    Piece board_[64];
    
    uint8_t castling_;
    Square ep_square_;
    uint8_t halfmove_clock_;
    uint8_t fullmove_number_;
    Color side_to_move_;
    
    Bitboard checkers_;
    Bitboard pinned_;
    Bitboard slider_attackers_;
    
    uint64_t key_;
};

#endif // POSITION_H
