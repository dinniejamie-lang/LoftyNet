#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"
#include <cstdint>
#include <string>

using Bitboard = uint64_t;

// Bitboard operations
inline Bitboard bitboard_set(Bitboard bb, Square s) {
    return bb | (1ULL << s);
}

inline Bitboard bitboard_clear(Bitboard bb, Square s) {
    return bb & ~(1ULL << s);
}

inline bool bitboard_is_set(Bitboard bb, Square s) {
    return bb & (1ULL << s);
}

inline int bitboard_count(Bitboard bb) {
    return __builtin_popcountll(bb);
}

inline Square bitboard_lsb(Bitboard bb) {
    return static_cast<Square>(__builtin_ctzll(bb));
}

inline Square bitboard_msb(Bitboard bb) {
    return static_cast<Square>(63 - __builtin_clzll(bb));
}

inline Bitboard bitboard_pop_lsb(Bitboard bb) {
    return bb & (bb - 1);
}

inline bool bitboard_is_empty(Bitboard bb) {
    return bb == 0;
}

// File and rank masks
extern const Bitboard FILE_MASKS[8];
extern const Bitboard RANK_MASKS[8];
extern const Bitboard CENTER_MASK;
extern const Bitboard DARK_SQUARES;
extern const Bitboard LIGHT_SQUARES;

// Utility functions
inline std::string square_to_string(Square s) {
    if (s == SQ_NONE) return "none";
    char file = 'a' + square_file(s);
    char rank = '1' + square_rank(s);
    return std::string(1, file) + rank;
}

inline std::string bitboard_to_string(Bitboard bb) {
    std::string result;
    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            result += bitboard_is_set(bb, sq) ? '1' : '.';
        }
        result += '\n';
    }
    return result;
}

// Precomputed attack tables
extern Bitboard PAWN_ATTACKS[2][64];  // [color][square]
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard BETWEEN_BB[64][64];    // squares between two squares
extern Bitboard LINE_BB[64][64];       // line through two squares

void init_bitboards();

#endif // BITBOARD_H
