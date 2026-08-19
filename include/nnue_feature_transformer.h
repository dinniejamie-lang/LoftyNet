#ifndef NNUE_FEATURE_TRANSFORMER_H
#define NNUE_FEATURE_TRANSFORMER_H

#include "nnue_types.h"
#include "position.h"
#include <array>
#include <vector>
#include <cstdint>

namespace nnue {

// Feature Transformer (HalfKP architecture)
// Maps input features to hidden layer activations
class FeatureTransformer {
public:
    FeatureTransformer();
    
    // Initialize from position
    void initialize(const Position& pos);
    
    // Incremental update for a move
    void update(const Position& pos, const AccumulatorState& parent_state);
    
    // Get current accumulator state
    const AccumulatorState& get_state() const { return state_; }
    
    // Get activated outputs (after ReLU)
    const std::array<int16_t, HIDDEN_SIZE_1>& get_white_output() const;
    const std::array<int16_t, HIDDEN_SIZE_1>& get_black_output() const;
    
    // Refresh entire accumulator (slow, used when incremental fails)
    void refresh(const Position& pos);
    
    // Initialize weights with heuristic patterns (public for Network access)
    void initialize_weights();
    
private:
    AccumulatorState state_;
    
    // Weight matrices (initialized with heuristic values)
    std::array<int16_t, HIDDEN_SIZE_1 * INPUT_SIZE> weights_;
    std::array<int16_t, HIDDEN_SIZE_1> biases_;
    
    // Add/subtract features from accumulator
    void add_feature(int feature_idx);
    void subtract_feature(int feature_idx);
    
    // Apply activation function
    void apply_activation();
};

} // namespace nnue

#endif // NNUE_FEATURE_TRANSFORMER_H
