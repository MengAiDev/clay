#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace clay {

class Watcher {
public:
    using EventCallback = std::function<void(const std::string& path, bool isDir)>;
    
    Watcher(const std::string& path, 
            const std::vector<std::string>& ignorePatterns,
            EventCallback callback);
    ~Watcher();
    
    void start();
    void stop();
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace clay