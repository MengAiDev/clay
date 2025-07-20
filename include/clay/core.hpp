#pragma once

#include <string>
#include <vector>
#include <memory>

namespace clay {

class Core {
public:
    static Core& instance();

    bool isInitialized() const;  // 新增此方法
    bool init(const std::string& workspace);
    void run();
    void shutdown();
    
    void takeSnapshot(bool autoSave = false, const std::string& message = "");
    bool restoreSnapshot(const std::string& snapshotId);
    bool undo();
    std::vector<std::string> listSnapshots();
    std::string currentSnapshotId() const;
    
    void createTempBranch();
    void commitTempBranch(const std::string& name);
    void discardTempBranch();

    std::string getDiff(const std::string& snapshotId) const;
    std::string findClosestSnapshot(const std::string& targetTime) const;

private:
    Core();
    ~Core();
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
    bool initialized_ = false;  // 新增此成员
};

} // namespace clay