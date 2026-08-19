#include "nnue_evaluation.h"

NnueEvaluation::NnueEvaluation() : use_nnue_(true) {
}

void NnueEvaluation::initialize() {
    if (!network_) {
        network_ = std::make_unique<nnue::Network>();
        network_->initialize();
    }
}

Score NnueEvaluation::evaluate(const Position& pos) {
    if (!use_nnue_ || !network_) {
        // Fall back to classical evaluation
        return Evaluation::evaluate(pos);
    }
    
    // Use NNUE evaluation
    int16_t nnue_score = network_->evaluate(pos);
    
    // Add small classical term for endgame precision
    Score classical_material = Evaluation::evaluate_material(pos, WHITE) - 
                               Evaluation::evaluate_material(pos, BLACK);
    
    // Blend: 90% NNUE + 10% classical material
    return static_cast<Score>((nnue_score * 9 + classical_material) / 10);
}

EvalTerms NnueEvaluation::get_terms(const Position& pos) {
    // For now, just return classical terms
    // In a full implementation, we'd decompose the NNUE evaluation
    return Evaluation::get_terms(pos);
}

void NnueEvaluation::use_nnue(bool enable) {
    use_nnue_ = enable;
}

bool NnueEvaluation::using_nnue() const {
    return use_nnue_;
}
