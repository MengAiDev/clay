#pragma once

#include <string>
#include <vector>

namespace clay {

class Command {
public:
    static int execute(const std::vector<std::string>& args);
    
private:
    static void init(const std::vector<std::string>& args);
    static void status();
    static void timeline();
    static void rewind(const std::vector<std::string>& args);
    static void undo();
    static void branch(const std::vector<std::string>& args);
    static void commit(const std::vector<std::string>& args);
    static void help();
};

} // namespace clay