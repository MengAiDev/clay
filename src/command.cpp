#include "clay/command.hpp"
#include "clay/core.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <ctime>

namespace clay {

int Command::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        help();
        return 1;
    }
    
    std::string command = args[0];
    
    try {
        if (command == "init") {
            init(args);
        } else if (command == "status") {
            status();
        } else if (command == "timeline") {
            timeline();
        } else if (command == "rewind") {
            rewind(args);
        } else if (command == "undo") {
            undo();
        } else if (command == "branch") {
            branch(args);
        } else if (command == "commit") {
            commit(args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            help();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

void Command::init(const std::vector<std::string>& args) {
    std::string path = ".";
    if (args.size() > 1) {
        path = args[1];
    }
    
    if (!Core::instance().init(path)) {
        throw std::runtime_error("Failed to initialize Clay repository");
    }
    
    std::cout << "Initialized Clay repository at " << path << std::endl;
}

void Command::status() {
    if (!Core::instance().isInitialized()) {
        throw std::runtime_error("Clay repository not initialized. Run 'clay init' first.");
    }
    std::cout << "Current snapshot: " << Core::instance().currentSnapshotId() << std::endl;
}

void Command::timeline() {
    if (!Core::instance().isInitialized()) {
        throw std::runtime_error("Clay repository not initialized. Run 'clay init' first.");
    }
    auto snapshots = Core::instance().listSnapshots();
    
    if (snapshots.empty()) {
        std::cout << "No snapshots available" << std::endl;
        return;
    }
    
    std::cout << "Snapshot Timeline:" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;
    for (const auto& snap : snapshots) {
        std::cout << snap << std::endl;
    }
}

void Command::rewind(const std::vector<std::string>& args) {
    if (!Core::instance().isInitialized()) {
        throw std::runtime_error("Clay repository not initialized. Run 'clay init' first.");
    }
    if (args.size() < 2) {
        throw std::runtime_error("Usage: clay rewind <snapshot-id|time>");
    }
    
    std::string target = args[1];
    
    // 尝试作为时间解析 (HH:MM)
    if (target.find(':') != std::string::npos) {
        // TODO: 实现时间查找
        std::cout << "Rewinding to time: " << target << std::endl;
    } 
    // 尝试作为相对时间 (5min, 10min)
    else if (target.find("min") != std::string::npos) {
        // TODO: 实现相对时间查找
        std::cout << "Rewinding " << target << std::endl;
    }
    // 作为快照ID
    else {
        if (!Core::instance().restoreSnapshot(target)) {
            throw std::runtime_error("Failed to restore snapshot: " + target);
        }
        std::cout << "Restored snapshot: " << target << std::endl;
    }
}

void Command::undo() {
    if (!Core::instance().isInitialized()) {
        throw std::runtime_error("Clay repository not initialized. Run 'clay init' first.");
    }
    if (!Core::instance().undo()) {
        throw std::runtime_error("Failed to undo last change");
    }
    std::cout << "Undo successful" << std::endl;
}

void Command::branch(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw std::runtime_error("Usage: clay branch [--temp|--keep <name>]");
    }
    
    if (args[1] == "--temp") {
        Core::instance().createTempBranch();
        std::cout << "Created temporary branch" << std::endl;
    } 
    else if (args[1] == "--keep" && args.size() > 2) {
        Core::instance().commitTempBranch(args[2]);
        std::cout << "Committed branch as: " << args[2] << std::endl;
    }
    else {
        throw std::runtime_error("Invalid branch command");
    }
}

void Command::commit(const std::vector<std::string>& args) {
    std::string message = "Manual snapshot";
    if (args.size() > 1) {
        message = args[1];
    }
    
    Core::instance().takeSnapshot(false, message);
    std::cout << "Created manual snapshot" << std::endl;
}

void Command::help() {
    std::cout << "Clay - Lightweight Version Control for Rapid Prototyping\n";
    std::cout << "Usage: clay <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  init [path]      Initialize a new repository\n";
    std::cout << "  status           Show current status\n";
    std::cout << "  timeline         List all snapshots\n";
    std::cout << "  rewind <target>  Restore to snapshot or time (e.g., clay rewind 14:30)\n";
    std::cout << "  undo             Undo last change\n";
    std::cout << "  branch --temp    Create temporary in-memory branch\n";
    std::cout << "  branch --keep <name> Commit temp branch as permanent\n";
    std::cout << "  commit [msg]     Create manual snapshot\n";
}

} // namespace clay