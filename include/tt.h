#ifndef TT_H
#define TT_H

#include "move.h"
#include "evaluation.h"
#include <cstdint>
#include <cstddef>
#include <vector>

// Transposition table entry flags
enum TTFlag : uint8_t {
    TT_EXACT = 0,   // Exact score
    TT_LOWER = 1,   // Lower bound (beta cutoff)
    TT_UPPER = 2    // Upper bound (alpha failed high)
};

// TT entry - 16 bytes
struct TTEntry {
    uint64_t key;       // Position hash
    Move best_move;     // Best move found
    int16_t score;      // Score (can be mate distance)
    uint8_t depth;      // Search depth
    TTFlag flag;        // Entry type
    uint8_t age;        // For replacement
    
    bool is_valid() const { return key != 0; }
    void clear() { key = 0; }
};

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();
    
    // Set table size in MB
    void set_size(size_t mb);
    
    // Clear the table
    void clear();
    
    // Store an entry
    void store(uint64_t key, Move move, Score score, int depth, TTFlag flag, int ply);
    
    // Probe the table
    TTEntry* probe(uint64_t key);
    
    // Get best move for a position
    Move get_move(uint64_t key);
    
    // Increment age (called each search)
    void new_search();
    
    // Statistics
    size_t size() const { return num_entries_; }
    size_t hits() const { return hits_; }
    size_t misses() const { return misses_; }
    
private:
    std::vector<TTEntry> entries_;
    size_t num_entries_;
    uint8_t current_age_;
    size_t hits_;
    size_t misses_;
};

extern TranspositionTable TT;

#endif // TT_H
