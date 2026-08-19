#ifndef NNUE_EVALUATION_H
#define NNUE_EVALUATION_H

#include "nnue_network.h"
#include "evaluation.h"
#include <memory>

// Hybrid evaluation combining classical and NNUE
class NnueEvaluation {
public:
    NnueEvaluation();
    
    // Initialize the network
    void initialize();
    
    // Evaluate position using NNUE
    Score evaluate(const Position& pos);
    
    // Get detailed terms (for debugging/analysis)
    EvalTerms get_terms(const Position& pos);
    
    // Check if NNUE is available
    bool is_available() const { return network_ != nullptr; }
    
    // Switch between classical and NNUE evaluation
    void use_nnue(bool enable);
    bool using_nnue() const { return use_nnue_; }
    
private:
    std::unique_ptr<nnue::Network> network_;
    bool use_nnue_;
};

#endif // NNUE_EVALUATION_H
