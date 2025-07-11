#include "clay/command.hpp"
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    return clay::Command::execute(args);
}