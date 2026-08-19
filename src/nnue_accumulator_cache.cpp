#include "nnue_accumulator_cache.h"
#include <cstring>

namespace nnue {

AccumulatorCache::AccumulatorCache() : current_depth_(0), valid_(false) {
}

void AccumulatorCache::push() {
    if (current_depth_ < MAX_DEPTH - 1) {
        // Copy current state to next depth level
        stack_[current_depth_ + 1] = stack_[current_depth_];
        current_depth_++;
    }
}

void AccumulatorCache::pop() {
    if (current_depth_ > 0) {
        current_depth_--;
    }
}

const AccumulatorState& AccumulatorCache::current_state() const {
    return stack_[current_depth_];
}

const AccumulatorState& AccumulatorCache::state_at(int depth) const {
    return stack_[depth];
}

void AccumulatorCache::initialize(const Position& pos) {
    current_depth_ = 0;
    valid_ = false;
    
    // Reset all states
    for (int i = 0; i < MAX_DEPTH; ++i) {
        stack_[i].reset();
    }
}

bool AccumulatorCache::needs_refresh() const {
    return !valid_;
}

void AccumulatorCache::set_valid(bool valid) {
    valid_ = valid;
}

} // namespace nnue
