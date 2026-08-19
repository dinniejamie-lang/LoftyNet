#ifndef MOVE_H
#define MOVE_H

#include "types.h"
#include <cstdint>

// Move encoding: 16 bits
// Bits 0-5: from square (6 bits)
// Bits 6-11: to square (6 bits)
// Bits 12-13: move type (0=normal, 1=promotion, 2=en passant, 3=castling)
// Bits 14-15: promotion piece (if promotion)

enum MoveType : uint8_t {
    MOVE_NORMAL = 0,
    MOVE_PROMOTION = 1,
    MOVE_EN_PASSANT = 2,
    MOVE_CASTLING = 3
};

class Move {
public:
    Move() : data(0) {}
    
    Move(Square from, Square to, MoveType mt = MOVE_NORMAL, PieceType pt = KNIGHT)
        : data(static_cast<uint16_t>(from | (to << 6) | (mt << 12) | (pt << 14))) {}
    
    Square from_sq() const { return static_cast<Square>(data & 0x3F); }
    Square to_sq() const { return static_cast<Square>((data >> 6) & 0x3F); }
    MoveType type() const { return static_cast<MoveType>((data >> 12) & 0x3); }
    PieceType promotion_type() const { return static_cast<PieceType>((data >> 14) & 0x3); }
    
    bool is_null() const { return data == 0; }
    bool operator==(const Move& other) const { return data == other.data; }
    bool operator!=(const Move& other) const { return data != other.data; }
    
    static Move null() { return Move(); }
    
private:
    uint16_t data;
};

// Move list for generation
constexpr int MAX_MOVES = 256;

struct MoveList {
    Move moves[MAX_MOVES];
    int count = 0;
    
    void add(Move m) {
        if (count < MAX_MOVES) {
            moves[count++] = m;
        }
    }
    
    void clear() { count = 0; }
    
    Move& operator[](int i) { return moves[i]; }
    const Move& operator[](int i) const { return moves[i]; }
    
    int size() const { return count; }
    
    using iterator = Move*;
    using const_iterator = const Move*;
    
    iterator begin() { return moves; }
    iterator end() { return moves + count; }
    const_iterator begin() const { return moves; }
    const_iterator end() const { return moves + count; }
};

#endif // MOVE_H
