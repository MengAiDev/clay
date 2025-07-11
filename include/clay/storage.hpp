#pragma once

#include "snapshot.hpp"
#include <string>
#include <vector>
#include <memory>

namespace clay {

class Storage {
public:
    Storage(const std::string& workspace);
    ~Storage();
    
    bool init();
    std::string store(const Snapshot& snapshot);
    Snapshot load(const std::string& snapshotId) const;
    std::vector<Snapshot> list() const;
    bool remove(const std::string& snapshotId);
    void cleanup();
    
    std::string lastSnapshotId() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace clay