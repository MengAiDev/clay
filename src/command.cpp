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
        } else if (command == "diff") {
            diff(args, out);
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
    out << "  timeline         List all snapshots\n";
    out << "  rewind <target>  Restore to snapshot or time (e.g., clay rewind 14:30)\n";
    out << "  undo             Undo last change\n";
    out << "  branch --temp    Create temporary in-memory branch\n";
    out << "  branch --keep <name> Commit temp branch as permanent\n";
    out << "  commit [msg]     Create manual snapshot\n";
    out << "  diff <time>      Show differences for snapshot at specified time\n";
}

void Command::diff(const std::vector<std::string>& args, std::ostream& out) {
    if (args.size() < 2) {
        throw std::runtime_error("Usage: clay diff <snapshot-time|snapshot-id>");
    }
    
    // 合并所有参数（解决带空格的时间格式问题）
    std::string target;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!target.empty()) target += " ";
        target += args[i];
    }
    
    std::string targetId;
    auto snapshots = Core::instance().listSnapshots();
    bool found = false;
    
    // 1. 首先尝试直接作为ID匹配
    for (const auto& snapStr : snapshots) {
        size_t idEnd = snapStr.find(' ');
        if (idEnd != std::string::npos) {
            std::string id = snapStr.substr(0, idEnd);
            if (id == target) {
                targetId = id;
                found = true;
                break;
            }
        }
    }
    
    // 2. 尝试作为时间匹配
    if (!found) {
        try {
            // 使用核心功能查找最接近的快照
            targetId = Core::instance().findClosestSnapshot(target);
            found = true;
        } catch (...) {
            // 忽略错误，继续尝试其他方法
        }
    }
    
    // 3. 如果还是没找到，尝试原始的时间字符串匹配
    if (!found) {
        for (const auto& snapStr : snapshots) {
            // 快照字符串格式: "20250711 | 2025-07-11 11:18:49 | manual | 1"
            size_t firstPipe = snapStr.find('|');
            if (firstPipe == std::string::npos) continue;
            
            size_t timeStart = firstPipe + 1;
            while (timeStart < snapStr.size() && std::isspace(snapStr[timeStart])) {
                timeStart++;
            }
            
            size_t secondPipe = snapStr.find('|', timeStart);
            if (secondPipe == std::string::npos) continue;
            
            size_t timeEnd = secondPipe - 1;
            while (timeEnd > timeStart && std::isspace(snapStr[timeEnd])) {
                timeEnd--;
            }
            
            std::string snapTime = snapStr.substr(timeStart, timeEnd - timeStart + 1);
            
            if (snapTime == target) {
                size_t idEnd = snapStr.find(' ');
                targetId = snapStr.substr(0, idEnd);
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        throw std::runtime_error("No snapshot found for: " + target);
    }
    
    // 获取差异并输出
    std::string diffOutput = Core::instance().getDiff(targetId);
    out << diffOutput;
}
} // namespace clay