#include "nnue_weight_initializer.h"
#include <cmath>
#include <algorithm>

namespace nnue {

// Pre-defined chess patterns
const std::array<int8_t, 64> WeightInitializer::CENTER_BIAS = {
    0,  1,  2,  3,  3,  2,  1,  0,
    1,  3,  5,  6,  6,  5,  3,  1,
    2,  5,  8, 10, 10,  8,  5,  2,
    3,  6, 10, 12, 12, 10,  6,  3,
    3,  6, 10, 12, 12, 10,  6,  3,
    2,  5,  8, 10, 10,  8,  5,  2,
    1,  3,  5,  6,  6,  5,  3,  1,
    0,  1,  2,  3,  3,  2,  1,  0
};

const std::array<int8_t, 64> WeightInitializer::KING_SHELT_BIAS = {
    5,  4,  3,  2,  2,  3,  4,  5,
    4,  3,  2,  1,  1,  2,  3,  4,
    3,  2,  1,  0,  0,  1,  2,  3,
    2,  1,  0, -1, -1,  0,  1,  2,
    2,  1,  0, -1, -1,  0,  1,  2,
    3,  2,  1,  0,  0,  1,  2,  3,
    4,  3,  2,  1,  1,  2,  3,  4,
    5,  4,  3,  2,  2,  3,  4,  5
};

const std::array<int8_t, 64> WeightInitializer::PASSED_PAWN_BIAS = {
    0,  0,  0,  0,  0,  0,  0,  0,
    2,  3,  4,  5,  5,  4,  3,  2,
    4,  5,  7,  8,  8,  7,  5,  4,
    6,  8, 10, 12, 12, 10,  8,  6,
    8, 10, 12, 14, 14, 12, 10,  8,
   10, 12, 14, 16, 16, 14, 12, 10,
   12, 14, 16, 18, 18, 16, 14, 12,
   14, 16, 18, 20, 20, 18, 16, 14
};

int16_t WeightInitializer::get_pst_value(PieceType pt, Square sq, Color c) {
    // Simplified piece-square table values
    static const int16_t pawn_pst[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
       50, 50, 50, 50, 50, 50, 50, 50,
       10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    };
    
    static const int16_t knight_pst[64] = {
       -50,-40,-30,-30,-30,-30,-40,-50,
       -40,-20,  0,  0,  0,  0,-20,-40,
       -30,  0, 10, 15, 15, 10,  0,-30,
       -30,  5, 15, 20, 20, 15,  5,-30,
       -30,  0, 15, 20, 20, 15,  0,-30,
       -30,  5, 10, 15, 15, 10,  5,-30,
       -40,-20,  0,  5,  5,  0,-20,-40,
       -50,-40,-30,-30,-30,-30,-40,-50
    };
    
    static const int16_t bishop_pst[64] = {
       -20,-10,-10,-10,-10,-10,-10,-20,
       -10,  0,  0,  0,  0,  0,  0,-10,
       -10,  0,  5, 10, 10,  5,  0,-10,
       -10,  5,  5, 10, 10,  5,  5,-10,
       -10,  0, 10, 10, 10, 10,  0,-10,
       -10, 10, 10, 10, 10, 10, 10,-10,
       -10,  5,  0,  0,  0,  0,  5,-10,
       -20,-10,-10,-10,-10,-10,-10,-20
    };
    
    static const int16_t rook_pst[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10, 10, 10, 10, 10,  5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
       -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0
    };
    
    static const int16_t queen_pst[64] = {
       -20,-10,-10, -5, -5,-10,-10,-20,
       -10,  0,  0,  0,  0,  0,  0,-10,
       -10,  0,  5,  5,  5,  5,  0,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
         0,  0,  5,  5,  5,  5,  0, -5,
       -10,  5,  5,  5,  5,  5,  0,-10,
       -10,  0,  5,  0,  0,  0,  0,-10,
       -20,-10,-10, -5, -5,-10,-10,-20
    };
    
    static const int16_t king_pst[64] = {
       -30,-40,-40,-50,-50,-40,-40,-30,
       -30,-40,-40,-50,-50,-40,-40,-30,
       -30,-40,-40,-50,-50,-40,-40,-30,
       -30,-40,-40,-50,-50,-40,-40,-30,
       -20,-30,-30,-40,-40,-30,-30,-20,
       -10,-20,-20,-20,-20,-20,-20,-10,
        20, 20,  0,  0,  0,  0, 20, 20,
        20, 30, 10,  0,  0, 10, 30, 20
    };
    
    Square adjusted_sq = (c == WHITE) ? sq : Square(63 - static_cast<int>(sq));
    int idx = static_cast<int>(adjusted_sq);
    
    switch (pt) {
        case PAWN: return pawn_pst[idx];
        case KNIGHT: return knight_pst[idx];
        case BISHOP: return bishop_pst[idx];
        case ROOK: return rook_pst[idx];
        case QUEEN: return queen_pst[idx];
        case KING: return king_pst[idx];
        default: return 0;
    }
}

float WeightInitializer::xavier_init(int fan_in, int fan_out, std::mt19937& gen) {
    float limit = std::sqrt(6.0f / (fan_in + fan_out));
    std::uniform_real_distribution<float> dist(-limit, limit);
    return dist(gen) * SCALE_FACTOR;
}

float WeightInitializer::he_init(int fan_in, std::mt19937& gen) {
    float limit = std::sqrt(2.0f / fan_in);
    std::normal_distribution<float> dist(0.0f, limit);
    return dist(gen) * SCALE_FACTOR;
}

void WeightInitializer::initialize_feature_transformer(
    std::array<int16_t, HIDDEN_SIZE_1 * INPUT_SIZE>& weights,
    std::array<int16_t, HIDDEN_SIZE_1>& biases) {
    
    std::mt19937 gen(42); // Fixed seed for reproducibility
    
    // Initialize with heuristic patterns based on chess knowledge
    weights.fill(0);
    biases.fill(64); // Base bias
    
    // Assign different neurons to different chess concepts
    int neuron_offset = 0;
    
    // Neurons 0-63: Piece-square tables for each piece type
    for (int pt = 0; pt < 6; ++pt) {
        PieceType piece_type = static_cast<PieceType>(pt);
        generate_piece_patterns(
            reinterpret_cast<std::array<int16_t, HIDDEN_SIZE_1>&>(
                *reinterpret_cast<std::array<int16_t, HIDDEN_SIZE_1>*>(&weights[neuron_offset * HIDDEN_SIZE_1])
            ),
            piece_type,
            neuron_offset * 4  // Spread across neurons
        );
        neuron_offset += 4;
    }
    
    // Neurons for king safety
    generate_king_safety_patterns(
        reinterpret_cast<std::array<int16_t, HIDDEN_SIZE_1>&>(
            *reinterpret_cast<std::array<int16_t, HIDDEN_SIZE_1>*>(&weights[neuron_offset * HIDDEN_SIZE_1])
        ),
        neuron_offset * 4
    );
    neuron_offset += 4;
    
    // Neurons for pawn structure
    generate_pawn_structure_patterns(
        reinterpret_cast<std::array<int16_t, HIDDEN_SIZE_1>&>(
            *reinterpret_cast<std::array<int16_t, HIDDEN_SIZE_1>*>(&weights[neuron_offset * HIDDEN_SIZE_1])
        ),
        neuron_offset * 4
    );
    neuron_offset += 4;
    
    // Fill remaining with small random values
    for (size_t i = neuron_offset * HIDDEN_SIZE_1; i < weights.size(); ++i) {
        weights[i] = static_cast<int16_t>(he_init(INPUT_SIZE, gen));
    }
}

void WeightInitializer::initialize_fc1(
    std::array<int16_t, HIDDEN_SIZE_2 * HIDDEN_SIZE_1>& weights,
    std::array<int16_t, HIDDEN_SIZE_2>& biases) {
    
    std::mt19937 gen(42);
    
    // Xavier initialization scaled for quantization
    for (size_t i = 0; i < weights.size(); ++i) {
        weights[i] = static_cast<int16_t>(xavier_init(HIDDEN_SIZE_1 * 2, HIDDEN_SIZE_2, gen));
    }
    
    biases.fill(static_cast<int16_t>(SCALE_FACTOR * 0.1f));
}

void WeightInitializer::initialize_fc2(
    std::array<int16_t, OUTPUT_SIZE * HIDDEN_SIZE_2>& weights,
    std::array<int16_t, OUTPUT_SIZE>& biases) {
    
    std::mt19937 gen(42);
    
    // Smaller initialization for output layer
    for (size_t i = 0; i < weights.size(); ++i) {
        weights[i] = static_cast<int16_t>(xavier_init(HIDDEN_SIZE_2, OUTPUT_SIZE, gen) * 0.5f);
    }
    
    biases.fill(0);
}

void WeightInitializer::generate_piece_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern, 
                                                 PieceType pt, int base_neuron) {
    // Create patterns that respond to piece positions
    for (int sq = 0; sq < 64; ++sq) {
        int feature_idx = make_feature_index(Square(sq), pt, WHITE);
        if (feature_idx >= 0 && feature_idx < INPUT_SIZE) {
            int16_t pst_value = get_pst_value(pt, Square(sq), WHITE);
            
            // Distribute pattern across multiple neurons
            for (int n = 0; n < 4 && (base_neuron + n) < HIDDEN_SIZE_1; ++n) {
                int neuron_idx = base_neuron + n;
                int weight = pst_value * CENTER_BIAS[sq] * (n + 1) / 4;
                pattern[neuron_idx] = static_cast<int16_t>(std::clamp(weight, -127, 127));
            }
        }
    }
}

void WeightInitializer::generate_king_safety_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern,
                                                       int base_neuron) {
    // Create patterns sensitive to king safety
    for (int sq = 0; sq < 64; ++sq) {
        int king_feature = make_feature_index(Square(sq), KING, WHITE);
        if (king_feature >= 0) {
            int safety_value = KING_SHELT_BIAS[sq];
            
            for (int n = 0; n < 4 && (base_neuron + n) < HIDDEN_SIZE_1; ++n) {
                int neuron_idx = base_neuron + n;
                int weight = safety_value * (n + 1) * 2;
                pattern[neuron_idx] = static_cast<int16_t>(std::clamp(weight, -127, 127));
            }
        }
    }
}

void WeightInitializer::generate_pawn_structure_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern,
                                                          int base_neuron) {
    // Create patterns for passed pawns and pawn structure
    for (int sq = 0; sq < 64; ++sq) {
        int pawn_feature = make_feature_index(Square(sq), PAWN, WHITE);
        if (pawn_feature >= 0) {
            int passed_value = PASSED_PAWN_BIAS[sq];
            
            for (int n = 0; n < 4 && (base_neuron + n) < HIDDEN_SIZE_1; ++n) {
                int neuron_idx = base_neuron + n;
                int weight = passed_value * (n + 1) / 2;
                pattern[neuron_idx] = static_cast<int16_t>(std::clamp(weight, -127, 127));
            }
        }
    }
}

void WeightInitializer::generate_mobility_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern,
                                                    int base_neuron) {
    // Mobility is handled implicitly through piece activity
    // Higher weights for central squares = more mobility
    for (int sq = 0; sq < 64; ++sq) {
        for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
            int feature = make_feature_index(Square(sq), static_cast<PieceType>(pt), WHITE);
            if (feature >= 0) {
                int mobility_value = CENTER_BIAS[sq];
                
                for (int n = 0; n < 2 && (base_neuron + n) < HIDDEN_SIZE_1; ++n) {
                    int neuron_idx = base_neuron + n;
                    int weight = mobility_value * (pt + 1);
                    pattern[neuron_idx] = static_cast<int16_t>(std::clamp(weight, -127, 127));
                }
            }
        }
    }
}

} // namespace nnue
