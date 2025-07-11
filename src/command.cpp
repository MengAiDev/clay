#include "clay/command.hpp"
#include "clay/core.hpp"
#include <vector>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace clay {

int Command::execute(const std::vector<std::string>& args, std::ostream& out) {
    if (args.empty()) {
        help(out);
        return 1;
    }
    
    std::string command = args[0];
    
    try {
        if (command == "init") {
            // 由main.cpp直接处理
            out << "Initialized Clay repository" << std::endl;
        } else if (command == "status") {
            status(out);
        } else if (command == "timeline") {
            timeline(out);
        } else if (command == "rewind") {
            rewind(args, out);
        } else if (command == "undo") {
            undo(out);
        } else if (command == "branch") {
            branch(args, out);
        } else if (command == "commit") {
            commit(args, out);
        } else if (command == "stop") {
            // 由main.cpp直接处理
            out << "Stopping Clay daemon" << std::endl;
        } else {
            out << "Unknown command: " << command << std::endl;
            help(out);
            return 1;
        }
    } catch (const std::exception& e) {
        out << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

void Command::init(const std::vector<std::string>& args, std::ostream& out) {
    std::string path = ".";
    if (args.size() > 1) {
        path = args[1];
    }
    
    if (!Core::instance().init(path)) {
        throw std::runtime_error("Failed to initialize Clay repository");
    }
    
    out << "Initialized Clay repository at " << path << std::endl;
}

void Command::status(std::ostream& out) {
    out << "Current snapshot: " << Core::instance().currentSnapshotId() << std::endl;
}

void Command::timeline(std::ostream& out) {
    auto snapshots = Core::instance().listSnapshots();
    
    if (snapshots.empty()) {
        out << "No snapshots available" << std::endl;
        return;
    }
    
    out << "Snapshot Timeline:" << std::endl;
    out << "------------------------------------------------" << std::endl;
    for (const auto& snap : snapshots) {
        out << snap << std::endl;
    }
}

void Command::rewind(const std::vector<std::string>& args, std::ostream& out) {
    if (args.size() < 2) {
        throw std::runtime_error("Usage: clay rewind <snapshot-id|time>");
    }
    
    std::string target = args[1];
    
    // 尝试作为时间解析 (HH:MM)
    if (target.find(':') != std::string::npos) {
        // TODO: 实现时间查找
        out << "Rewinding to time: " << target << std::endl;
    } 
    // 尝试作为相对时间 (5min, 10min)
    else if (target.find("min") != std::string::npos) {
        // TODO: 实现相对时间查找
        out << "Rewinding " << target << std::endl;
    }
    // 作为快照ID
    else {
        if (!Core::instance().restoreSnapshot(target)) {
            throw std::runtime_error("Failed to restore snapshot: " + target);
        }
        out << "Restored snapshot: " << target << std::endl;
    }
}

void Command::undo(std::ostream& out) {
    if (!Core::instance().undo()) {
        throw std::runtime_error("Failed to undo last change");
    }
    out << "Undo successful" << std::endl;
}

void Command::branch(const std::vector<std::string>& args, std::ostream& out) {
    if (args.size() < 2) {
        throw std::runtime_error("Usage: clay branch [--temp|--keep <name>]");
    }
    
    if (args[1] == "--temp") {
        Core::instance().createTempBranch();
        out << "Created temporary branch" << std::endl;
    } 
    else if (args[1] == "--keep" && args.size() > 2) {
        Core::instance().commitTempBranch(args[2]);
        out << "Committed branch as: " << args[2] << std::endl;
    }
    else {
        throw std::runtime_error("Invalid branch command");
    }
}

void Command::commit(const std::vector<std::string>& args, std::ostream& out) {
    std::string message = "Manual snapshot";
    if (args.size() > 1) {
        message = args[1];
    }
    
    Core::instance().takeSnapshot(false, message);
    out << "Created manual snapshot" << std::endl;
}

void Command::help(std::ostream& out) {
    out << "Clay - Lightweight Version Control for Rapid Prototyping\n";
    out << "Usage: clay <command> [options]\n\n";
    out << "Commands:\n";
    out << "  init [path]      Initialize a new repository\n";
    out << "  stop             Stop the Clay daemon\n";
    out << "  status           Show current status\n";
    out << "  timeline         List all snapshots\n";
    out << "  rewind <target>  Restore to snapshot or time (e.g., clay rewind 14:30)\n";
    out << "  undo             Undo last change\n";
    out << "  branch --temp    Create temporary in-memory branch\n";
    out << "  branch --keep <name> Commit temp branch as permanent\n";
    out << "  commit [msg]     Create manual snapshot\n";
}

} // namespace clay