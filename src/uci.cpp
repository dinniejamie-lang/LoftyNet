#include "uci.h"
#include "movegen.h"
#include <iostream>
#include <sstream>
#include <cstring>

UCI::UCI() : searching_(false) {
    search_ = &search_engine;
}

void UCI::run() {
    std::string line;
    
    std::cout << "id name ChessEngine" << std::endl;
    std::cout << "id author Developer" << std::endl;
    std::cout << "option name Hash type spin default 16 min 1 max 2048" << std::endl;
    std::cout << "uciok" << std::endl;
    
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        execute(line);
    }
}

void UCI::execute(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string token;
    iss >> token;
    
    if (token == "uci") {
        cmd_uci();
    } else if (token == "isready") {
        cmd_isready();
    } else if (token == "ucinewgame") {
        cmd_ucinewgame();
    } else if (token == "position") {
        cmd_position(cmd);
    } else if (token == "go") {
        cmd_go(cmd);
    } else if (token == "stop") {
        cmd_stop();
    } else if (token == "setoption") {
        cmd_setoption(cmd);
    }
}

void UCI::cmd_uci() {
    std::cout << "id name ChessEngine" << std::endl;
    std::cout << "id author Developer" << std::endl;
    std::cout << "option name Hash type spin default 16 min 1 max 2048" << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCI::cmd_isready() {
    std::cout << "readyok" << std::endl;
}

void UCI::cmd_ucinewgame() {
    TT.clear();
    position_.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    search_->set_position(position_);
}

void UCI::cmd_position(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string token;
    iss >> token; // Skip "position"
    
    position_.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    iss >> token;
    if (token == "startpos") {
        // Already set
    } else if (token == "fen") {
        std::string fen;
        std::getline(iss, fen);
        position_.set_fen(fen);
    }
    
    // Check for "moves"
    if (iss >> token && token == "moves") {
        std::string move_str;
        while (iss >> move_str) {
            // Parse move and make it
            MoveList moves;
            MoveGenerator::generate_all(position_, moves);
            
            for (int i = 0; i < moves.size(); ++i) {
                Move m = moves[i];
                std::string ms = square_to_string(m.from_sq()) + square_to_string(m.to_sq());
                
                // Handle promotions
                if (move_str.length() > 4) {
                    char promo = move_str[4];
                    if (promo == 'q' || promo == 'r' || promo == 'b' || promo == 'n') {
                        ms += promo;
                    }
                }
                
                if (ms == move_str || 
                    (move_str.length() == 5 && ms + move_str[4] == move_str)) {
                    State st;
                    position_.save_state(st);
                    position_.make_move(m);
                    break;
                }
            }
        }
    }
    
    search_->set_position(position_);
}

void UCI::cmd_go(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string token;
    iss >> token; // Skip "go"
    
    SearchLimits limits;
    limits.max_depth = 100; // Default max depth
    
    while (iss >> token) {
        int value;
        if (token == "depth" && (iss >> value)) {
            limits.max_depth = value;
        } else if (token == "movetime" && (iss >> value)) {
            limits.max_time_ms = value;
        } else if (token == "wtime" && (iss >> value)) {
            if (position_.side_to_move() == WHITE) {
                limits.max_time_ms = value / 20;
            }
        } else if (token == "btime" && (iss >> value)) {
            if (position_.side_to_move() == BLACK) {
                limits.max_time_ms = value / 20;
            }
        } else if (token == "infinite") {
            limits.infinite = true;
        } else if (token == "nodes" && (iss >> value)) {
            limits.max_nodes = value;
        }
    }
    
    if (!limits.use_time() && !limits.infinite && limits.max_nodes == 0) {
        limits.max_time_ms = 1000; // Default 1 second
    }
    
    searching_ = true;
    SearchResult result = search_->search(position_, limits);
    searching_ = false;
    
    std::cout << "bestmove " << square_to_string(result.best_move.from_sq())
              << square_to_string(result.best_move.to_sq());
    
    if (result.best_move.type() == MOVE_PROMOTION) {
        char p = 'q';
        switch (result.best_move.promotion_type()) {
            case ROOK: p = 'r'; break;
            case BISHOP: p = 'b'; break;
            case KNIGHT: p = 'n'; break;
            default: p = 'q';
        }
        std::cout << p;
    }
    std::cout << std::endl;
}

void UCI::cmd_stop() {
    search_->stop();
    searching_ = false;
}

void UCI::cmd_quit() {
    // Handled in run()
}

void UCI::cmd_setoption(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string token, name, value;
    
    iss >> token; // Skip "setoption"
    iss >> token; // Skip "name"
    iss >> name;
    iss >> token; // "value"
    iss >> value;
    
    if (name == "Hash" && !value.empty()) {
        int mb = std::stoi(value);
        TT.set_size(mb);
    }
}

void UCI::print_bestmove(Move m) {
    std::cout << "bestmove " << square_to_string(m.from_sq())
              << square_to_string(m.to_sq()) << std::endl;
}

void UCI::print_info(const SearchResult& result, int depth) {
    std::cout << "info depth " << depth 
              << " score cp " << result.score
              << " nodes " << result.nodes
              << " pv " << result.pv << std::endl;
}

int main() {
    UCI uci;
    uci.run();
    return 0;
}
