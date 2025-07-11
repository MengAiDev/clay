#pragma once

#include <string>
#include <vector>
#include <memory>

namespace clay {

class Core {
public:
    static Core& instance();

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

private:
    Core();
    ~Core();
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace clay