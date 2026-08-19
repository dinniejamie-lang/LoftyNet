#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "move.h"
#include "tt.h"
#include <cstdint>
#include <vector>
#include <string>

// Search limits
struct SearchLimits {
    int max_depth = 0;
    int max_time_ms = 0;
    int64_t max_nodes = 0;
    bool infinite = false;
    
    bool use_time() const { return max_time_ms > 0; }
};

// Search result
struct SearchResult {
    Move best_move;
    Score score;
    int depth;
    int64_t nodes;
    uint64_t nps;
    std::string pv;
    
    SearchResult() : best_move(), score(SCORE_NONE), depth(0), nodes(0), nps(0) {}
};

// Search stack entry (for recursion)
struct SearchStack {
    Move current_move;
    Move excluded_move;
    int static_eval;
    int ply;
};

class Search {
public:
    Search();
    
    // Main search function
    SearchResult search(const Position& pos, const SearchLimits& limits);
    
    // Stop searching
    void stop();
    
    // Set current position for UCI
    void set_position(const Position& pos);
    
    // Is searching active
    bool is_searching() const { return searching_; }
    
    // Statistics
    int64_t nodes() const { return nodes_; }
    int current_depth() const { return current_depth_; }
    
private:
    // Core search functions
    Score search_root(int depth, Color color);
    Score alpha_beta(int depth, Score alpha, Score beta, Color color, bool cut_node);
    Score quiescence(Score alpha, Score beta, Color color);
    
    // Move ordering
    void order_moves(MoveList& moves, const Position& pos, Move tt_move, int ply);
    int score_move(Move m, const Position& pos, Move tt_move, int ply);
    
    // Extensions and reductions
    int extension(const Position& pos, Move m, int ply);
    int reduction(Move m, int depth, int ply, bool improving);
    
    // Check if we should stop
    bool should_stop() const;
    
    // Principal variation
    void update_pv(Move m, Color color);
    std::string get_pv(int depth);
    
    // Position and state
    Position root_pos_;
    Position current_pos_;
    std::vector<SearchStack> stack_;
    
    // Search parameters
    SearchLimits limits_;
    SearchResult result_;
    
    // Counters
    int64_t nodes_;
    int current_depth_;
    int best_move_changes_;
    uint64_t start_time_;
    
    // Control
    volatile bool stopping_;
    volatile bool searching_;
    
    // Move ordering tables
    Move killer_moves[2][128];      // [ply][index]
    int history_table[2][64][64];   // [color][from][to]
    int capture_history[12][64];    // [piece_type][to]
};

extern Search search_engine;

#endif // SEARCH_H
