#ifndef NNUE_TYPES_H
#define NNUE_TYPES_H

#include "types.h"
#include <cstdint>
#include <array>
#include <vector>
#include <cstring>
#include <cmath>

// NNUE Configuration
namespace nnue {

// Network architecture
constexpr int INPUT_SIZE = 768;        // 64 squares * 12 piece types
constexpr int HIDDEN_SIZE_1 = 256;     // First hidden layer
constexpr int HIDDEN_SIZE_2 = 32;      // Second hidden layer  
constexpr int OUTPUT_SIZE = 1;         // Single output (evaluation)

// Quantization parameters
constexpr int QUANTIZATION_BITS = 8;
constexpr float SCALE_FACTOR = 127.0f;

// Feature transformer configuration
constexpr int FEATURE_BUFFER_SIZE = 4096;

// Piece type encoding for NNUE
enum class NNUEPieceType : uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NONE = 6
};

// Convert internal piece type to NNUE piece type
inline NNUEPieceType to_nnue_piece(PieceType pt) {
    switch (pt) {
        case PAWN: return NNUEPieceType::PAWN;
        case KNIGHT: return NNUEPieceType::KNIGHT;
        case BISHOP: return NNUEPieceType::BISHOP;
        case ROOK: return NNUEPieceType::ROOK;
        case QUEEN: return NNUEPieceType::QUEEN;
        case KING: return NNUEPieceType::KING;
        default: return NNUEPieceType::NONE;
    }
}

// Feature index calculation
inline int make_feature_index(Square sq, PieceType pt, Color c) {
    if (pt == NO_PIECE_TYPE || sq >= 64) return -1;
    
    int piece_idx = static_cast<int>(to_nnue_piece(pt));
    int color_idx = (c == WHITE) ? 0 : 6;
    int square_idx = (c == WHITE) ? static_cast<int>(sq) : (63 - static_cast<int>(sq));
    
    return (color_idx + piece_idx) * 64 + square_idx;
}

// Accumulator state for incremental updates
struct AccumulatorState {
    std::array<int16_t, HIDDEN_SIZE_1> white_accum;
    std::array<int16_t, HIDDEN_SIZE_1> black_accum;
    bool active;
    
    AccumulatorState() : active(false) {
        white_accum.fill(0);
        black_accum.fill(0);
    }
    
    void reset() {
        white_accum.fill(0);
        black_accum.fill(0);
        active = false;
    }
};

// Network layer types
struct WeightMatrix {
    std::array<float, HIDDEN_SIZE_1 * INPUT_SIZE> weights;
    std::array<float, HIDDEN_SIZE_1> biases;
};

struct HiddenLayer1 {
    std::array<float, HIDDEN_SIZE_2 * HIDDEN_SIZE_1> weights;
    std::array<float, HIDDEN_SIZE_2> biases;
};

struct HiddenLayer2 {
    std::array<float, OUTPUT_SIZE * HIDDEN_SIZE_2> weights;
    std::array<float, OUTPUT_SIZE> biases;
};

// Activation functions
inline float relu(float x) {
    return (x > 0.0f) ? x : 0.0f;
}

inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

inline float clipped_relu(float x, float limit = 127.0f) {
    return (x > 0.0f) ? ((x < limit) ? x : limit) : 0.0f;
}

} // namespace nnue

#endif // NNUE_TYPES_H
