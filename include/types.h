#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <array>

// Piece types
enum PieceType : uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NO_PIECE_TYPE = 6
};

// Colors
enum Color : uint8_t {
    WHITE = 0,
    BLACK = 1,
    COLOR_NONE = 2
};

inline Color operator!(Color c) { return static_cast<Color>(c ^ 1); }

// Files and ranks
enum File : uint8_t {
    FILE_A = 0, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NONE
};

enum Rank : uint8_t {
    RANK_1 = 0, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NONE
};

// Square representation (0-63)
enum Square : uint8_t {
    A1 = 0, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE = 64
};

inline Square make_square(File f, Rank r) {
    return static_cast<Square>(f + r * 8);
}

inline File square_file(Square s) {
    return static_cast<File>(s & 7);
}

inline Rank square_rank(Square s) {
    return static_cast<Rank>(s >> 3);
}

inline int square_index(Square s) {
    return static_cast<int>(s);
}

// Piece representation
enum Piece : uint8_t {
    W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    NO_PIECE = 0
};

inline PieceType type_of(Piece p) {
    if (p == NO_PIECE) return NO_PIECE_TYPE;
    return static_cast<PieceType>((p > 8 ? p - 8 : p) - 1);
}

inline Color color_of(Piece p) {
    if (p == NO_PIECE) return COLOR_NONE;
    return p > 8 ? BLACK : WHITE;
}

inline Piece make_piece(Color c, PieceType pt) {
    return static_cast<Piece>((c == BLACK ? 8 : 0) + pt + 1);
}

// Direction offsets
enum Direction : int8_t {
    NORTH = 8, SOUTH = -8, EAST = 1, WEST = -1,
    NORTH_EAST = 9, NORTH_WEST = 7, SOUTH_EAST = -7, SOUTH_WEST = -9
};

#endif // TYPES_H
