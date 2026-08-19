#ifndef UCI_H
#define UCI_H

#include "position.h"
#include "search.h"
#include <string>

class UCI {
public:
    UCI();
    
    // Main UCI loop
    void run();
    
    // Parse and execute a UCI command
    void execute(const std::string& cmd);
    
private:
    // UCI commands
    void cmd_uci();
    void cmd_isready();
    void cmd_ucinewgame();
    void cmd_position(const std::string& cmd);
    void cmd_go(const std::string& cmd);
    void cmd_stop();
    void cmd_quit();
    void cmd_setoption(const std::string& cmd);
    
    // Helper functions
    void print_bestmove(Move m);
    void print_info(const SearchResult& result, int depth);
    
    Position position_;
    Search* search_;
    bool searching_;
};

#endif // UCI_H
