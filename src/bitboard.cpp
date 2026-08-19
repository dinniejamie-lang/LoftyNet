#include "bitboard.h"
#include <cstring>

// File and rank masks
const Bitboard FILE_MASKS[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
};

const Bitboard RANK_MASKS[8] = {
    0xFFULL, 0xFF00ULL, 0xFF0000ULL, 0xFF000000ULL,
    0xFF00000000ULL, 0xFF0000000000ULL, 0xFF000000000000ULL, 0xFF00000000000000ULL
};

const Bitboard CENTER_MASK = 0x0000001818000000ULL;
const Bitboard DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
const Bitboard LIGHT_SQUARES = 0x55AA55AA55AA55AAULL;

// Attack tables
Bitboard PAWN_ATTACKS[2][64];
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard BETWEEN_BB[64][64];
Bitboard LINE_BB[64][64];

void init_bitboards() {
    // Pawn attacks
    for (int c = 0; c < 2; ++c) {
        for (int sq = 0; sq < 64; ++sq) {
            File f = square_file(static_cast<Square>(sq));
            Rank r = square_rank(static_cast<Square>(sq));
            
            if (c == WHITE) {
                if (f > FILE_A && r < RANK_7)
                    PAWN_ATTACKS[c][sq] |= (1ULL << make_square(static_cast<File>(f - 1), static_cast<Rank>(r + 1)));
                if (f < FILE_H && r < RANK_7)
                    PAWN_ATTACKS[c][sq] |= (1ULL << make_square(static_cast<File>(f + 1), static_cast<Rank>(r + 1)));
            } else {
                if (f > FILE_A && r > RANK_1)
                    PAWN_ATTACKS[c][sq] |= (1ULL << make_square(static_cast<File>(f - 1), static_cast<Rank>(r - 1)));
                if (f < FILE_H && r > RANK_1)
                    PAWN_ATTACKS[c][sq] |= (1ULL << make_square(static_cast<File>(f + 1), static_cast<Rank>(r - 1)));
            }
        }
    }
    
    // Knight attacks
    for (int sq = 0; sq < 64; ++sq) {
        File f = square_file(static_cast<Square>(sq));
        Rank r = square_rank(static_cast<Square>(sq));
        
        static const int knight_offsets[8][2] = {
            {1, 2}, {-1, 2}, {1, -2}, {-1, -2},
            {2, 1}, {2, -1}, {-2, 1}, {-2, -1}
        };
        
        for (int d = 0; d < 8; ++d) {
            int nf = f + knight_offsets[d][0];
            int nr = r + knight_offsets[d][1];
            if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8)
                KNIGHT_ATTACKS[sq] |= (1ULL << make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
        }
    }
    
    // King attacks
    for (int sq = 0; sq < 64; ++sq) {
        File f = square_file(static_cast<Square>(sq));
        Rank r = square_rank(static_cast<Square>(sq));
        
        for (int df = -1; df <= 1; ++df) {
            for (int dr = -1; dr <= 1; ++dr) {
                if (df == 0 && dr == 0) continue;
                int nf = f + df, nr = r + dr;
                if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8)
                    KING_ATTACKS[sq] |= (1ULL << make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
            }
        }
    }
    
    // Between and line BBs
    for (int s1 = 0; s1 < 64; ++s1) {
        for (int s2 = 0; s2 < 64; ++s2) {
            if (s1 == s2) continue;
            
            Square sq1 = static_cast<Square>(s1);
            Square sq2 = static_cast<Square>(s2);
            
            // Check if on same rank, file, or diagonal
            File f1 = square_file(sq1), f2 = square_file(sq2);
            Rank r1 = square_rank(sq1), r2 = square_rank(sq2);
            
            bool same_rank = (r1 == r2);
            bool same_file = (f1 == f2);
            bool same_diag = ((r1 + f1) == (r2 + f2)) || ((r1 - f1) == (r2 - f2));
            
            if (same_rank || same_file || same_diag) {
                // Calculate line
                int df = (f2 > f1) ? 1 : (f2 < f1) ? -1 : 0;
                int dr = (r2 > r1) ? 1 : (r2 < r1) ? -1 : 0;
                
                Square s = sq1;
                do {
                    s = make_square(static_cast<File>(square_file(s) + df), 
                                    static_cast<Rank>(square_rank(s) + dr));
                    LINE_BB[s1][s2] |= (1ULL << s);
                } while (s != sq2);
                
                // Between is line without endpoints
                BETWEEN_BB[s1][s2] = LINE_BB[s1][s2] & ~((1ULL << sq1) | (1ULL << sq2));
            }
        }
    }
}
