#include "clay/core.hpp"
#include "clay/snapshot.hpp"
#include "clay/storage.hpp"
#include "clay/watcher.hpp"
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <mutex>
#include <atomic>
#include <ctime>
#include <filesystem>
#include <unordered_set>
#include <regex>

namespace fs = std::filesystem;
using namespace std::chrono;

namespace clay {

class Core::Impl {
public:
    Impl() 
        : workspace_(), 
          storage_(nullptr),
          watcher_(nullptr),
          running_(false),
          lastActivity_(steady_clock::now()),
          lastSnapshotTime_(steady_clock::now()),
          tempBranchActive_(false) {}
    
    bool init(const std::string& workspace) {
        workspace_ = workspace;
        fs::path clayDir = workspace_ / ".clay";
        
        if (!fs::exists(clayDir)) {
            if (!fs::create_directory(clayDir)) {
                std::cerr << "Failed to create .clay directory" << std::endl;
                return false;
            }
        }
        
        fs::path confPath = clayDir / "clay.conf";
        if (!fs::exists(confPath)) {
            std::ofstream confFile(confPath);
            if (confFile) {
                confFile << DEFAULT_CONFIG;
                confFile.close();
            }
        }
        
        storage_ = std::make_unique<Storage>(workspace_);
        if (!storage_->init()) {
            std::cerr << "Failed to initialize storage" << std::endl;
            return false;
        }
        
        loadIgnorePatterns();
        return true;
    }
    
    void run() {
        running_ = true;
        
        watcher_ = std::make_unique<Watcher>(
            workspace_.string(),
            ignorePatterns_,
            [this](const std::string& path, bool isDir) {
                if (!isDir) lastActivity_ = steady_clock::now();
            }
        );
        watcher_->start();
        
        while (running_) {
            auto now = steady_clock::now();
            
            if (duration_cast<seconds>(now - lastActivity_.load()).count() < idleThreshold_) {
                if (duration_cast<seconds>(now - lastSnapshotTime_.load()).count() >= autosaveInterval_) {
                    takeSnapshot(true);
                    lastSnapshotTime_ = now;
                }
            }
            
            std::this_thread::sleep_for(seconds(1));
        }
        
        watcher_->stop();
    }
    
    void shutdown() { running_ = false; }
    
    void takeSnapshot(bool autoSave, const std::string& message = "") {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        
        auto t = system_clock::to_time_t(system_clock::now());
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&t));
        std::string snapshotId = buf;
        
        Snapshot snapshot;
        snapshot.id = snapshotId;
        snapshot.timestamp = t;
        snapshot.autoSave = autoSave;
        snapshot.message = message.empty() ? generateAutoMessage() : message;
        
        captureFileSystemState(snapshot);
        storage_->store(snapshot);
        
        lastSnapshotTime_ = steady_clock::now();
        std::cout << "Snapshot created: " << snapshotId << std::endl;
    }
    
    bool restoreSnapshot(const std::string& snapshotId) {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        
        try {
            Snapshot snapshot = storage_->load(snapshotId);
            
            // 修复：正确删除所有现有文件（保留.clay目录）
            for (const auto& entry : fs::directory_iterator(workspace_)) {
                // 跳过 .clay 目录
                if (entry.path().filename() == ".clay") continue;
                
                // 删除文件或目录
                fs::remove_all(entry.path());
            }
            
            // 恢复快照中的文件
            for (const auto& delta : snapshot.deltas) {
                fs::path fullPath = workspace_ / delta.path;
                
                switch (delta.action) {
                    case FileDelta::CREATE:
                    case FileDelta::MODIFY: {
                        fs::create_directories(fullPath.parent_path());
                        std::ofstream file(fullPath, std::ios::binary);
                        file.write(reinterpret_cast<const char*>(delta.content.data()), delta.content.size());
                        break;
                    }
                        
                    case FileDelta::DELETE:
                        if (fs::exists(fullPath)) fs::remove(fullPath);
                        break;
                }
            }
            
            std::cout << "Restored snapshot: " << snapshotId << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to restore snapshot: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool undo() {
        std::vector<Snapshot> snapshots = storage_->list();
        if (snapshots.size() < 2) {
            throw std::runtime_error("Need at least 2 snapshots to undo");
        }
        
        Snapshot prev = snapshots[snapshots.size() - 2];
        return restoreSnapshot(prev.id);
    }
    
    std::vector<std::string> listSnapshots() {
        std::vector<Snapshot> snapshots = storage_->list();
        std::vector<std::string> result;
        
        for (const auto& s : snapshots) {
            std::ostringstream oss;
            oss << s.shortId() << " | " << s.timeString() 
                << " | " << (s.autoSave ? "auto" : "manual") 
                << " | " << s.summary();
            result.push_back(oss.str());
        }
        
        return result;
    }
    
    std::string currentSnapshotId() const {
        return storage_->lastSnapshotId();
    }
    
    void createTempBranch() {
        if (tempBranchActive_) {
            std::cerr << "Temp branch already active" << std::endl;
            return;
        }
        tempBranchActive_ = true;
    }
    
    void commitTempBranch(const std::string& name) {
        if (!tempBranchActive_) {
            std::cerr << "No active temp branch" << std::endl;
            return;
        }
        tempBranchActive_ = false;
    }
    
    void discardTempBranch() {
        if (!tempBranchActive_) {
            std::cerr << "No active temp branch" << std::endl;
            return;
        }
        tempBranchActive_ = false;
    }

private:
    void captureFileSystemState(Snapshot& snapshot) {
        for (const auto& entry : fs::recursive_directory_iterator(workspace_)) {
            if (fs::is_directory(entry)) continue;
            
            std::string relPath = fs::relative(entry.path(), workspace_).string();
            if (isIgnored(relPath)) continue;
            
            std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
            if (!file) continue;
            
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::vector<uint8_t> buffer(size);
            if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                snapshot.deltas.emplace_back(relPath, FileDelta::MODIFY, buffer);
            }
        }
    }
    
    void loadIgnorePatterns() {
        fs::path confPath = workspace_ / ".clay" / "clay.conf";
        if (!fs::exists(confPath)) return;
        
        std::ifstream confFile(confPath);
        std::string line;
        
        while (std::getline(confFile, line)) {
            if (line.empty() || line[0] == '#' || line[0] == '[') continue;
            
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            
            if (key == "autosave_interval") {
                autosaveInterval_ = std::stoi(value);
            } else if (key == "idle_threshold") {
                idleThreshold_ = std::stoi(value);
            } else if (key == "max_snapshots") {
                maxSnapshots_ = std::stoi(value);
            } else if (key == "ignore_patterns") {
                size_t start = 0, end;
                while ((end = value.find(',', start)) != std::string::npos) {
                    ignorePatterns_.push_back(trim(value.substr(start, end - start)));
                    start = end + 1;
                }
                ignorePatterns_.push_back(trim(value.substr(start)));
            }
        }
    }
    
    bool isIgnored(const std::string& path) const {
        fs::path relPath = fs::relative(path, workspace_);
        
        for (const auto& pattern : ignorePatterns_) {
            if (pattern.find('*') != std::string::npos) {
                std::string regexPattern = std::regex_replace(
                    pattern, 
                    std::regex("\\*"), 
                    ".*"
                );
                std::regex re(regexPattern, std::regex::icase);
                if (std::regex_match(relPath.string(), re)) {
                    return true;
                }
            } else if (relPath == pattern) {
                return true;
            }
        }
        return false;
    }
    
    std::string generateAutoMessage() const {
        return "Auto snapshot at " + std::to_string(time(nullptr));
    }
    
    std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), 
            [](unsigned char c){ return std::isspace(c); });
        auto end = std::find_if_not(str.rbegin(), str.rend(), 
            [](unsigned char c){ return std::isspace(c); }).base();
        return (start < end) ? std::string(start, end) : "";
    }

    fs::path workspace_;
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<Watcher> watcher_;
    
    std::atomic<bool> running_;
    std::atomic<steady_clock::time_point> lastActivity_;
    std::atomic<steady_clock::time_point> lastSnapshotTime_;
    
    int autosaveInterval_ = 30;
    int idleThreshold_ = 5;
    int maxSnapshots_ = 100;
    std::vector<std::string> ignorePatterns_;
    
    bool tempBranchActive_ = false;
    std::mutex snapshotMutex_;
    
    static constexpr const char* DEFAULT_CONFIG = R"(
[core]
autosave_interval = 30
idle_threshold = 5
max_snapshots = 100
ignore_patterns = *.tmp, *.swp, build/, .git/
)";
};

Core& Core::instance() {
    static Core instance;
    return instance;
}

Core::Core() : impl_(std::make_unique<Impl>()) {}
Core::~Core() = default;

bool Core::init(const std::string& workspace) { return impl_->init(workspace); }
void Core::run() { impl_->run(); }
void Core::shutdown() { impl_->shutdown(); }
void Core::takeSnapshot(bool autoSave, const std::string& message) { impl_->takeSnapshot(autoSave, message); }
bool Core::restoreSnapshot(const std::string& snapshotId) { return impl_->restoreSnapshot(snapshotId); }
bool Core::undo() { 
    return impl_->undo(); 
}

std::vector<std::string> Core::listSnapshots() { return impl_->listSnapshots(); }
std::string Core::currentSnapshotId() const {
     return impl_->currentSnapshotId(); 
}
void Core::createTempBranch() { 
    impl_->createTempBranch(); 
}

void Core::commitTempBranch(const std::string& name) { 
    impl_->commitTempBranch(name); 
}
void Core::discardTempBranch() { impl_->discardTempBranch(); }

} // namespace clay