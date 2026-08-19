#include "nnue_network.h"
#include "nnue_simd.h"
#include <cmath>
#include <fstream>

namespace nnue {

Network::Network() : output_scale_(400) {
}

void Network::initialize() {
    feature_transformer_.initialize_weights();
    initialize_weights_heuristic();
}

void Network::initialize_weights_heuristic() {
    WeightInitializer::initialize_fc1(l1_weights_, l1_biases_);
    WeightInitializer::initialize_fc2(l2_weights_, l2_biases_);
}

bool Network::load_weights(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read l1 weights
    file.read(reinterpret_cast<char*>(l1_weights_.data()), 
              sizeof(int16_t) * l1_weights_.size());
    file.read(reinterpret_cast<char*>(l1_biases_.data()), 
              sizeof(int16_t) * l1_biases_.size());
    
    // Read l2 weights
    file.read(reinterpret_cast<char*>(l2_weights_.data()), 
              sizeof(int16_t) * l2_weights_.size());
    file.read(reinterpret_cast<char*>(l2_biases_.data()), 
              sizeof(int16_t) * l2_biases_.size());
    
    file.read(reinterpret_cast<char*>(&output_scale_), sizeof(int32_t));
    
    return file.good();
}

bool Network::save_weights(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write l1 weights
    file.write(reinterpret_cast<const char*>(l1_weights_.data()), 
               sizeof(int16_t) * l1_weights_.size());
    file.write(reinterpret_cast<const char*>(l1_biases_.data()), 
               sizeof(int16_t) * l1_biases_.size());
    
    // Write l2 weights
    file.write(reinterpret_cast<const char*>(l2_weights_.data()), 
               sizeof(int16_t) * l2_weights_.size());
    file.write(reinterpret_cast<const char*>(l2_biases_.data()), 
               sizeof(int16_t) * l2_biases_.size());
    
    file.write(reinterpret_cast<const char*>(&output_scale_), sizeof(int32_t));
    
    return file.good();
}

int16_t Network::evaluate(const Position& pos) {
    // Initialize or update feature transformer
    feature_transformer_.refresh(pos);
    
    const auto& white_accum = feature_transformer_.get_white_output();
    const auto& black_accum = feature_transformer_.get_black_output();
    
    return forward_pass(white_accum, black_accum, pos.side_to_move());
}

int16_t Network::evaluate_from_accumulator(const AccumulatorState& state, Color stm) {
    return forward_pass(state.white_accum, state.black_accum, stm);
}

int16_t Network::forward_pass(const std::array<int16_t, HIDDEN_SIZE_1>& white_accum,
                               const std::array<int16_t, HIDDEN_SIZE_1>& black_accum,
                               Color stm) {
    // Prepare combined input for FC layers
    // Concatenate white and black accumulators (with ReLU already applied)
    std::array<int16_t, HIDDEN_SIZE_1 * 2> combined;
    
    if (stm == WHITE) {
        for (int i = 0; i < HIDDEN_SIZE_1; ++i) {
            combined[i] = white_accum[i];
            combined[HIDDEN_SIZE_1 + i] = black_accum[i];
        }
    } else {
        for (int i = 0; i < HIDDEN_SIZE_1; ++i) {
            combined[i] = black_accum[i];
            combined[HIDDEN_SIZE_1 + i] = white_accum[i];
        }
    }
    
    // Layer 1: HIDDEN_SIZE_1 * 2 -> HIDDEN_SIZE_2
    std::array<int32_t, HIDDEN_SIZE_2> l1_output;
    for (int i = 0; i < HIDDEN_SIZE_2; ++i) {
        int32_t sum = l1_biases_[i];
        
        // Dot product with SIMD optimization
        const int16_t* weights_ptr = &l1_weights_[i * HIDDEN_SIZE_1 * 2];
        sum += simd::dot_product(combined.data(), weights_ptr, HIDDEN_SIZE_1 * 2);
        
        // Clipped ReLU activation
        l1_output[i] = static_cast<int16_t>(std::clamp(sum, 0, 127));
    }
    
    // Layer 2: HIDDEN_SIZE_2 -> OUTPUT_SIZE (1)
    int32_t output = l2_biases_[0];
    
    for (int i = 0; i < HIDDEN_SIZE_2; ++i) {
        output += static_cast<int32_t>(l1_output[i]) * static_cast<int32_t>(l2_weights_[i]);
    }
    
    // Scale output to centipawn range
    int16_t result = static_cast<int16_t>(output * output_scale_ / (127 * 127));
    
    // Clamp to reasonable evaluation range
    result = std::clamp(result, static_cast<int16_t>(-3000), static_cast<int16_t>(3000));
    
    return result;
}

std::string Network::get_info() const {
    return "NNUE Network: " + std::to_string(INPUT_SIZE) + " -> " + 
           std::to_string(HIDDEN_SIZE_1) + " -> " + 
           std::to_string(HIDDEN_SIZE_2) + " -> " + 
           std::to_string(OUTPUT_SIZE);
}

} // namespace nnue
