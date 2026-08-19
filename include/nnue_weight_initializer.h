#ifndef NNUE_WEIGHT_INITIALIZER_H
#define NNUE_WEIGHT_INITIALIZER_H

#include "nnue_types.h"
#include <array>
#include <cstdint>
#include <random>

namespace nnue {

// Weight initialization strategies for untrained network
// Uses chess knowledge to create meaningful initial weights
class WeightInitializer {
public:
    // Initialize feature transformer weights with piece-square table knowledge
    static void initialize_feature_transformer(
        std::array<int16_t, HIDDEN_SIZE_1 * INPUT_SIZE>& weights,
        std::array<int16_t, HIDDEN_SIZE_1>& biases);
    
    // Initialize FC layer 1 weights
    static void initialize_fc1(
        std::array<int16_t, HIDDEN_SIZE_2 * HIDDEN_SIZE_1>& weights,
        std::array<int16_t, HIDDEN_SIZE_2>& biases);
    
    // Initialize FC layer 2 (output) weights
    static void initialize_fc2(
        std::array<int16_t, OUTPUT_SIZE * HIDDEN_SIZE_2>& weights,
        std::array<int16_t, OUTPUT_SIZE>& biases);
    
    // Generate pattern-based weights for specific piece types
    static void generate_piece_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern, 
                                        PieceType pt, int base_neuron);
    
    // Generate king safety patterns
    static void generate_king_safety_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern,
                                              int base_neuron);
    
    // Generate pawn structure patterns
    static void generate_pawn_structure_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern,
                                                  int base_neuron);
    
    // Generate mobility patterns
    static void generate_mobility_patterns(std::array<int16_t, HIDDEN_SIZE_1>& pattern,
                                           int base_neuron);
    
private:
    // Helper to get PST value for a piece at a square
    static int16_t get_pst_value(PieceType pt, Square sq, Color c);
    
    // Xavier/He initialization scaled for quantized weights
    static float xavier_init(int fan_in, int fan_out, std::mt19937& gen);
    static float he_init(int fan_in, std::mt19937& gen);
    
    // Pre-defined chess patterns
    static const std::array<int8_t, 64> CENTER_BIAS;
    static const std::array<int8_t, 64> KING_SHELT_BIAS;
    static const std::array<int8_t, 64> PASSED_PAWN_BIAS;
};

} // namespace nnue

#endif // NNUE_WEIGHT_INITIALIZER_H
