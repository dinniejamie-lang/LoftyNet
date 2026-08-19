#ifndef NNUE_ACCUMULATOR_CACHE_H
#define NNUE_ACCUMULATOR_CACHE_H

#include "nnue_types.h"
#include "position.h"
#include <array>
#include <stack>
#include <cstdint>

namespace nnue {

// Accumulator cache for efficient search tree traversal
// Maintains a stack of accumulator states for backtracking
class AccumulatorCache {
public:
    static constexpr int MAX_DEPTH = 64;
    
    AccumulatorCache();
    
    // Push current state onto stack (before making a move)
    void push();
    
    // Pop state from stack (after undoing a move)
    void pop();
    
    // Get current depth
    int depth() const { return current_depth_; }
    
    // Get accumulator at current depth
    const AccumulatorState& current_state() const;
    
    // Get accumulator at specific depth
    const AccumulatorState& state_at(int depth) const;
    
    // Initialize for new search
    void initialize(const Position& pos);
    
    // Check if we need to refresh (incremental update not possible)
    bool needs_refresh() const;
    
    // Mark that incremental update is valid
    void set_valid(bool valid);
    
private:
    std::array<AccumulatorState, MAX_DEPTH> stack_;
    int current_depth_;
    bool valid_;
};

} // namespace nnue

#endif // NNUE_ACCUMULATOR_CACHE_H
