#include "tt.h"
#include <cstring>

TranspositionTable TT;

TranspositionTable::TranspositionTable() 
    : num_entries_(0), current_age_(0), hits_(0), misses_(0) {
    set_size(16); // Default 16 MB
}

TranspositionTable::~TranspositionTable() {
    entries_.clear();
}

void TranspositionTable::set_size(size_t mb) {
    size_t entries = (mb * 1024 * 1024) / sizeof(TTEntry);
    entries_.resize(entries);
    num_entries_ = entries;
    clear();
}

void TranspositionTable::clear() {
    for (auto& entry : entries_) {
        entry.clear();
    }
    current_age_ = 0;
    hits_ = 0;
    misses_ = 0;
}

void TranspositionTable::store(uint64_t key, Move move, Score score, int depth, TTFlag flag, int ply) {
    // Adjust mate scores based on distance from root
    if (score > -SCORE_MATE && score < -SCORE_DRAW) {
        score += ply;
    } else if (score > SCORE_DRAW && score < SCORE_MATE) {
        score -= ply;
    }
    
    size_t idx = key % num_entries_;
    TTEntry& entry = entries_[idx];
    
    // Always replace if empty or same key
    if (!entry.is_valid() || entry.key == key) {
        entry.key = key;
        entry.best_move = move;
        entry.score = static_cast<int16_t>(score);
        entry.depth = static_cast<uint8_t>(depth);
        entry.flag = flag;
        entry.age = current_age_;
        return;
    }
    
    // Replacement strategy: always replace if deeper or same age
    if (depth >= entry.depth || entry.age != current_age_) {
        entry.key = key;
        entry.best_move = move;
        entry.score = static_cast<int16_t>(score);
        entry.depth = static_cast<uint8_t>(depth);
        entry.flag = flag;
        entry.age = current_age_;
    }
}

TTEntry* TranspositionTable::probe(uint64_t key) {
    size_t idx = key % num_entries_;
    TTEntry& entry = entries_[idx];
    
    if (entry.is_valid() && entry.key == key) {
        hits_++;
        return &entry;
    }
    
    misses_++;
    return nullptr;
}

Move TranspositionTable::get_move(uint64_t key) {
    TTEntry* entry = probe(key);
    if (entry && entry->is_valid()) {
        return entry->best_move;
    }
    return Move::null();
}

void TranspositionTable::new_search() {
    current_age_++;
}
