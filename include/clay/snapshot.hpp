#pragma once

#include <string>
#include <ctime>
#include <cstdint>
#include <vector>

namespace clay {

struct FileDelta {
    std::string path;
    std::string baseSnapshotId;
    std::vector<uint8_t> delta;
    enum Action { ADDED, MODIFIED, DELETED } action;
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