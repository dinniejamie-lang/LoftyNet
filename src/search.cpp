#include "search.h"
#include "movegen.h"
#include "evaluation.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

Search search_engine;

Search::Search() 
    : nodes_(0), current_depth_(0), best_move_changes_(0), 
      start_time_(0), stopping_(false), searching_(false) {
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_table, 0, sizeof(history_table));
    memset(capture_history, 0, sizeof(capture_history));
    stack_.resize(128);
}

void Search::set_position(const Position& pos) {
    root_pos_ = pos;
}

SearchResult Search::search(const Position& pos, const SearchLimits& limits) {
    root_pos_ = pos;
    limits_ = limits;
    result_ = SearchResult();
    nodes_ = 0;
    current_depth_ = 0;
    best_move_changes_ = 0;
    stopping_ = false;
    searching_ = true;
    
    TT.new_search();
    start_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Iterative deepening
    for (int depth = 1; depth <= limits.max_depth && !stopping_; ++depth) {
        current_depth_ = depth;
        Score score = search_root(depth, root_pos_.side_to_move());
        
        if (!stopping_) {
            result_.score = score;
            result_.depth = depth;
            result_.nodes = nodes_;
            result_.pv = get_pv(depth);
            
            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            uint64_t elapsed = now - start_time_;
            result_.nps = (elapsed > 0) ? (nodes_ * 1000 / elapsed) : 0;
            
            std::cout << "info depth " << depth << " score cp " << score 
                      << " nodes " << nodes_ << " nps " << result_.nps 
                      << " pv " << result_.pv << std::endl;
        }
        
        // Check time
        if (limits.use_time()) {
            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (now - start_time_ >= static_cast<uint64_t>(limits.max_time_ms)) {
                stopping_ = true;
            }
        }
    }
    
    searching_ = false;
    return result_;
}

Score Search::search_root(int depth, Color color) {
    MoveList moves;
    MoveGenerator::generate_all(root_pos_, moves);
    
    if (moves.size() == 0) {
        if (root_pos_.in_check()) return -SCORE_MATE + 1;
        return SCORE_DRAW;
    }
    
    // Order moves
    Move tt_move = TT.get_move(root_pos_.key());
    order_moves(moves, root_pos_, tt_move, 0);
    
    Score best_score = -SCORE_MATE - 1;
    Move best_move = moves[0];
    
    for (int i = 0; i < moves.size(); ++i) {
        Move m = moves[i];
        
        State st;
        root_pos_.save_state(st);
        root_pos_.make_move(m);
        
        Score score;
        if (i == 0) {
            // First move: full search
            score = -alpha_beta(depth - 1, -SCORE_MATE - 1, -SCORE_MATE, !color, false);
        } else {
            // Other moves: null window search
            score = -alpha_beta(depth - 1, -best_score - 1, -best_score, !color, true);
            
            if (score > best_score && score < SCORE_MATE) {
                // Re-search with full window
                score = -alpha_beta(depth - 1, -SCORE_MATE - 1, -best_score, !color, false);
            }
        }
        
        root_pos_.unmake_move(m, st);
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
            if (score > -SCORE_MATE && score < SCORE_MATE) {
                update_pv(m, color);
            }
        }
        
        nodes_++;
        
        if (should_stop()) break;
    }
    
    result_.best_move = best_move;
    
    // Store in TT
    TTFlag flag = (best_score <= -SCORE_MATE) ? TT_UPPER : 
                  (best_score >= SCORE_MATE) ? TT_LOWER : TT_EXACT;
    TT.store(root_pos_.key(), best_move, best_score, depth, flag, 0);
    
    return best_score;
}

Score Search::alpha_beta(int depth, Score alpha, Score beta, Color color, bool cut_node) {
    // Check for stop
    if (should_stop()) return alpha;
    
    // Check for draw
    if (Evaluation::is_draw_material(root_pos_)) return SCORE_DRAW;
    
    // Transposition table lookup
    TTEntry* tt_entry = TT.probe(root_pos_.key());
    if (tt_entry && tt_entry->depth >= depth) {
        Score tt_score = tt_entry->score;
        
        // Adjust mate scores
        if (tt_score > -SCORE_MATE && tt_score < -SCORE_DRAW) tt_score -= 0;
        else if (tt_score > SCORE_DRAW && tt_score < SCORE_MATE) tt_score += 0;
        
        if (tt_entry->flag == TT_EXACT) return tt_score;
        if (tt_entry->flag == TT_LOWER && tt_score >= beta) return tt_score;
        if (tt_entry->flag == TT_UPPER && tt_score <= alpha) return tt_score;
    }
    
    // Quiescence search at leaf
    if (depth <= 0) {
        return quiescence(alpha, beta, color);
    }
    
    // Check extension
    bool in_check = root_pos_.in_check();
    if (in_check) depth++;
    
    MoveList moves;
    if (in_check) {
        MoveGenerator::generate_evasions(root_pos_, moves);
    } else {
        MoveGenerator::generate_all(root_pos_, moves);
    }
    
    if (moves.size() == 0) {
        if (in_check) return -SCORE_MATE + stack_[0].ply;
        return SCORE_DRAW;
    }
    
    // Get TT move for ordering
    Move tt_move = tt_entry ? tt_entry->best_move : Move::null();
    
    // Order moves
    order_moves(moves, root_pos_, tt_move, stack_[0].ply);
    
    Score best_score = -SCORE_MATE - 1;
    Move best_move = moves[0];
    int moves_searched = 0;
    
    for (int i = 0; i < moves.size(); ++i) {
        Move m = moves[i];
        
        // Skip TT move if already searched first
        if (i > 0 && m == tt_move) continue;
        
        // Late move reduction
        int r = 0;
        if (!in_check && moves_searched >= 4 && depth >= 3) {
            r = 1;
        }
        
        State st;
        root_pos_.save_state(st);
        root_pos_.make_move(m);
        
        Score score;
        if (moves_searched == 0) {
            // Principal variation search
            score = -alpha_beta(depth - 1, -beta, -alpha, !color, false);
        } else {
            // Null window search
            int new_depth = depth - 1 - r;
            score = -alpha_beta(new_depth, -alpha - 1, -alpha, !color, true);
            
            if (score > alpha && score < beta) {
                // Re-search
                score = -alpha_beta(depth - 1, -beta, -alpha, !color, false);
            }
        }
        
        root_pos_.unmake_move(m, st);
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
            
            if (score >= beta) {
                // Beta cutoff
                Piece captured = root_pos_.piece_at(m.to_sq());
                if (captured == NO_PIECE && m.type() != MOVE_EN_PASSANT) {
                    killer_moves[0][stack_[0].ply] = m;
                }
                break;
            }
            
            if (score > alpha) {
                alpha = score;
            }
        }
        
        moves_searched++;
        nodes_++;
    }
    
    // Store in TT
    TTFlag flag = (best_score <= alpha) ? TT_UPPER :
                  (best_score >= beta) ? TT_LOWER : TT_EXACT;
    TT.store(root_pos_.key(), best_move, best_score, depth, flag, stack_[0].ply);
    
    return best_score;
}

Score Search::quiescence(Score alpha, Score beta, Color color) {
    nodes_++;
    
    // Stand pat evaluation
    Score stand_pat = Evaluation::evaluate(root_pos_);
    
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;
    
    MoveList captures;
    MoveGenerator::generate_captures(root_pos_, captures);
    
    // Order captures by SEE
    for (int i = 0; i < captures.size(); ++i) {
        for (int j = i + 1; j < captures.size(); ++j) {
            if (root_pos_.see(captures[j]) > root_pos_.see(captures[i])) {
                Move tmp = captures[i];
                captures[i] = captures[j];
                captures[j] = tmp;
            }
        }
    }
    
    for (int i = 0; i < captures.size(); ++i) {
        Move m = captures[i];
        
        // Delta pruning
        if (stand_pat + root_pos_.see(m) + 200 <= alpha) continue;
        
        State st;
        root_pos_.save_state(st);
        root_pos_.make_move(m);
        
        Score score = -quiescence(-beta, -alpha, !color);
        
        root_pos_.unmake_move(m, st);
        
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    
    return alpha;
}

void Search::order_moves(MoveList& moves, const Position& pos, Move tt_move, int ply) {
    for (int i = 0; i < moves.size(); ++i) {
        for (int j = i + 1; j < moves.size(); ++j) {
            int score_i = score_move(moves[i], pos, tt_move, ply);
            int score_j = score_move(moves[j], pos, tt_move, ply);
            
            if (score_j > score_i) {
                Move tmp = moves[i];
                moves[i] = moves[j];
                moves[j] = tmp;
            }
        }
    }
}

int Search::score_move(Move m, const Position& pos, Move tt_move, int ply) {
    // TT move first
    if (m == tt_move) return 1000000;
    
    // Captures
    if (pos.piece_at(m.to_sq()) != NO_PIECE || m.type() == MOVE_EN_PASSANT) {
        return 900000 + pos.see(m);
    }
    
    // Killer moves
    if (m == killer_moves[0][ply]) return 800000;
    if (m == killer_moves[1][ply]) return 700000;
    
    // History
    Color c = pos.side_to_move();
    return history_table[c][m.from_sq()][m.to_sq()];
}

int Search::reduction(Move m, int depth, int ply, bool improving) {
    int r = 0;
    
    // Base reduction
    r = 1 + (depth >= 3 ? 1 : 0);
    
    // Non-improving
    if (!improving) r++;
    
    // Quiet moves
    Piece captured = root_pos_.piece_at(m.to_sq());
    if (captured == NO_PIECE && m.type() != MOVE_EN_PASSANT) r++;
    
    return std::min(r, depth - 1);
}

bool Search::should_stop() const {
    if (stopping_) return true;
    
    if (limits_.use_time()) {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - start_time_ >= static_cast<uint64_t>(limits_.max_time_ms)) {
            return true;
        }
    }
    
    if (limits_.max_nodes > 0 && nodes_ >= limits_.max_nodes) {
        return true;
    }
    
    return false;
}

void Search::update_pv(Move m, Color color) {
    // Simplified PV update
}

std::string Search::get_pv(int depth) {
    std::string pv;
    Position temp = root_pos_;
    
    for (int i = 0; i < depth && i < 50; ++i) {
        MoveList moves;
        MoveGenerator::generate_all(temp, moves);
        
        if (moves.size() == 0) break;
        
        // Find best move
        Move best = moves[0];
        Score best_score = -SCORE_MATE - 1;
        
        for (int j = 0; j < moves.size(); ++j) {
            State st;
            temp.save_state(st);
            temp.make_move(moves[j]);
            Score s = -Evaluation::evaluate(temp);
            temp.unmake_move(moves[j], st);
            
            if (s > best_score) {
                best_score = s;
                best = moves[j];
            }
        }
        
        if (pv.length() > 0) pv += " ";
        pv += square_to_string(best.from_sq());
        pv += square_to_string(best.to_sq());
        
        if (best.is_null()) break;
        
        State st;
        temp.save_state(st);
        temp.make_move(best);
    }
    
    return pv;
}

void Search::stop() {
    stopping_ = true;
    searching_ = false;
}
