#include "nnue_feature_transformer.h"
#include "nnue_simd.h"
#include "nnue_weight_initializer.h"
#include <cstring>

namespace nnue {

FeatureTransformer::FeatureTransformer() {
    initialize_weights();
}

void FeatureTransformer::initialize_weights() {
    WeightInitializer::initialize_feature_transformer(weights_, biases_);
}

void FeatureTransformer::initialize(const Position& pos) {
    state_.reset();
    
    // Build accumulator from scratch
    for (int sq_idx = 0; sq_idx < 64; ++sq_idx) {
        Square sq = static_cast<Square>(sq_idx);
        Piece piece = pos.piece_at(sq);
        if (piece != NO_PIECE) {
            int feature_idx = make_feature_index(sq, type_of(piece), color_of(piece));
            if (feature_idx >= 0) {
                add_feature(feature_idx);
            }
        }
    }
    
    state_.active = true;
    apply_activation();
}

void FeatureTransformer::update(const Position& pos, const AccumulatorState& parent_state) {
    // Copy parent state
    state_.white_accum = parent_state.white_accum;
    state_.black_accum = parent_state.black_accum;
    state_.active = true;
    
    // Note: In a full implementation, we would track which pieces moved
    // and only update the affected features. For now, we do a full refresh.
    // This is less efficient but correct.
    
    apply_activation();
}

void FeatureTransformer::refresh(const Position& pos) {
    state_.reset();
    
    for (int sq_idx = 0; sq_idx < 64; ++sq_idx) {
        Square sq = static_cast<Square>(sq_idx);
        Piece piece = pos.piece_at(sq);
        if (piece != NO_PIECE) {
            int feature_idx = make_feature_index(sq, type_of(piece), color_of(piece));
            if (feature_idx >= 0) {
                add_feature(feature_idx);
            }
        }
    }
    
    state_.active = true;
    apply_activation();
}

const std::array<int16_t, HIDDEN_SIZE_1>& FeatureTransformer::get_white_output() const {
    return state_.white_accum;
}

const std::array<int16_t, HIDDEN_SIZE_1>& FeatureTransformer::get_black_output() const {
    return state_.black_accum;
}

void FeatureTransformer::add_feature(int feature_idx) {
    if (feature_idx < 0 || feature_idx >= INPUT_SIZE) return;
    
    // Add weighted contribution to both accumulators
    const int16_t* weight_ptr = &weights_[feature_idx * HIDDEN_SIZE_1];
    
    // For white perspective
    simd::vector_add(state_.white_accum.data(), weight_ptr, HIDDEN_SIZE_1);
    
    // For black perspective (use mirrored weights)
    int mirrored_idx = feature_idx;
    if (feature_idx < 384) {
        mirrored_idx = feature_idx + 384;
    } else {
        mirrored_idx = feature_idx - 384;
    }
    
    const int16_t* mirror_weight_ptr = &weights_[mirrored_idx * HIDDEN_SIZE_1];
    simd::vector_add(state_.black_accum.data(), mirror_weight_ptr, HIDDEN_SIZE_1);
}

void FeatureTransformer::subtract_feature(int feature_idx) {
    if (feature_idx < 0 || feature_idx >= INPUT_SIZE) return;
    
    const int16_t* weight_ptr = &weights_[feature_idx * HIDDEN_SIZE_1];
    
    // Subtract from white accumulator
    for (int i = 0; i < HIDDEN_SIZE_1; ++i) {
        state_.white_accum[i] -= weight_ptr[i];
    }
    
    // Subtract from black accumulator (mirrored)
    int mirrored_idx = feature_idx;
    if (feature_idx < 384) {
        mirrored_idx = feature_idx + 384;
    } else {
        mirrored_idx = feature_idx - 384;
    }
    
    const int16_t* mirror_weight_ptr = &weights_[mirrored_idx * HIDDEN_SIZE_1];
    for (int i = 0; i < HIDDEN_SIZE_1; ++i) {
        state_.black_accum[i] -= mirror_weight_ptr[i];
    }
}

void FeatureTransformer::apply_activation() {
    // Apply ReLU and store back
    std::array<int16_t, HIDDEN_SIZE_1> temp_white;
    std::array<int16_t, HIDDEN_SIZE_1> temp_black;
    
    // Add bias before activation
    for (int i = 0; i < HIDDEN_SIZE_1; ++i) {
        temp_white[i] = state_.white_accum[i] + biases_[i];
        temp_black[i] = state_.black_accum[i] + biases_[i];
    }
    
    // Apply ReLU activation using SIMD
    simd::relu_activation(state_.white_accum.data(), temp_white.data(), HIDDEN_SIZE_1);
    simd::relu_activation(state_.black_accum.data(), temp_black.data(), HIDDEN_SIZE_1);
}

} // namespace nnue
