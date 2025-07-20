#pragma once
#include <string>
#include <vector>
#include <iostream>

namespace clay {

class Command {
public:
    static int execute(const std::vector<std::string>& args, std::ostream& out);
    
    static void init(const std::vector<std::string>& args, std::ostream& out);
    static void status(std::ostream& out);
    static void timeline(std::ostream& out);
    static void rewind(const std::vector<std::string>& args, std::ostream& out);
    static void undo(std::ostream& out);
    static void branch(const std::vector<std::string>& args, std::ostream& out);
    static void commit(const std::vector<std::string>& args, std::ostream& out);
    static void help(std::ostream& out);
    static void diff(const std::vector<std::string>& args, std::ostream& out); // New method for diff command
};

} // namespace clay