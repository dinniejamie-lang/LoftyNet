#ifndef NNUE_NETWORK_H
#define NNUE_NETWORK_H

#include "nnue_types.h"
#include "nnue_feature_transformer.h"
#include <array>
#include <cstdint>
#include <string>

namespace nnue {

// Full NNUE Network
// Combines feature transformer with fully connected layers
class Network {
public:
    Network();
    
    // Initialize network (load weights or use heuristics)
    void initialize();
    
    // Load weights from file (optional, for trained networks)
    bool load_weights(const std::string& filename);
    
    // Save weights to file
    bool save_weights(const std::string& filename);
    
    // Evaluate position
    int16_t evaluate(const Position& pos);
    
    // Evaluate with pre-computed accumulator
    int16_t evaluate_from_accumulator(const AccumulatorState& state, Color stm);
    
    // Get network info
    std::string get_info() const;
    
private:
    FeatureTransformer feature_transformer_;
    
    // Layer 1: HIDDEN_SIZE_1 -> HIDDEN_SIZE_2
    std::array<int16_t, HIDDEN_SIZE_2 * HIDDEN_SIZE_1> l1_weights_;
    std::array<int16_t, HIDDEN_SIZE_2> l1_biases_;
    
    // Layer 2: HIDDEN_SIZE_2 -> OUTPUT_SIZE (scaled output)
    std::array<int16_t, OUTPUT_SIZE * HIDDEN_SIZE_2> l2_weights_;
    std::array<int16_t, OUTPUT_SIZE> l2_biases_;
    
    // Output scale factor
    int32_t output_scale_;
    
    // Initialize weights with heuristic patterns
    void initialize_weights_heuristic();
    
    // Forward pass through FC layers
    int16_t forward_pass(const std::array<int16_t, HIDDEN_SIZE_1>& white_accum,
                         const std::array<int16_t, HIDDEN_SIZE_1>& black_accum,
                         Color stm);
};

} // namespace nnue

#endif // NNUE_NETWORK_H
