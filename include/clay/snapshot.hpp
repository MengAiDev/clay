#pragma once

#include <string>
#include <ctime>
#include <cstdint>
#include <vector>

namespace clay {

// 修改 FileDelta 结构，存储文件内容
struct FileDelta {
    enum Action { CREATE, MODIFY, DELETE };
    std::string path;
    std::vector<uint8_t> content; // 存储文件内容
    Action action;
    
    // 添加构造函数简化创建
    FileDelta(const std::string& p, Action a, const std::vector<uint8_t>& c = {})
        : path(p), action(a), content(c) {}
};

class Snapshot {
public:
    std::string id;
    std::time_t timestamp;
    bool autoSave;
    std::string message;
    std::vector<FileDelta> deltas; // 添加 deltas 成员

    std::string shortId() const {
        return id.substr(0, 8);
    }

    std::string timeString() const {
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));
        return buf;
    }

    std::string summary() const {
        return message.substr(0, 50) + (message.length() > 50 ? "..." : "");
    }
};

} // namespace clay