#include "clay/core.hpp"
#include "clay/snapshot.hpp"
#include "clay/storage.hpp"
#include "clay/watcher.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <mutex>
#include <atomic>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <cstring> // 添加 memset 头文件

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h> // 添加 ioctl 头文件
#endif
#include <regex>

namespace fs = std::filesystem;
using namespace std::chrono;

namespace clay {

class Core::Impl {
public:
    Impl() : 
        workspace_(), 
        storage_(nullptr),
        watcher_(nullptr),
        running_(false),
        lastActivity_(steady_clock::now()),
        lastSnapshotTime_(steady_clock::now()),
        tempBranchActive_(false) {}
    
    bool init(const std::string& workspace) {
        workspace_ = workspace;
        
        // 创建 .clay 目录
        fs::path clayDir = workspace_ / ".clay";
        if (!fs::exists(clayDir)) {
            if (!fs::create_directory(clayDir)) {
                std::cerr << "Failed to create .clay directory" << std::endl;
                return false;
            }
        }
        
        // 复制默认配置文件
        fs::path confPath = clayDir / "clay.conf";
        if (!fs::exists(confPath)) {
            std::ofstream confFile(confPath);
            if (confFile) {
                confFile << DEFAULT_CONFIG;
                confFile.close();
            }
        }
        
        // 初始化存储
        storage_ = std::make_unique<Storage>(workspace_);
        if (!storage_->init()) {
            std::cerr << "Failed to initialize storage" << std::endl;
            return false;
        }
        
        // 加载忽略模式
        loadIgnorePatterns();
        
        return true;
    }
    
    void run() {
        running_ = true;
        
        // 启动文件监控
        watcher_ = std::make_unique<Watcher>(
            workspace_.string(), // 转换为字符串
            ignorePatterns_,
            [this](const std::string& path, bool isDir) {
                if (isDir) return;
                if (isIgnored(path)) return;
                
                lastActivity_ = steady_clock::now();
            }
        );
        watcher_->start();
        
        // 主循环
        while (running_) {
            auto now = steady_clock::now();
            
            // 检查用户活动
            if (duration_cast<seconds>(now - lastActivity_.load()).count() < idleThreshold_) {
                // 检查是否达到自动保存间隔
                if (duration_cast<seconds>(now - lastSnapshotTime_.load()).count() >= autosaveInterval_) {
                    takeSnapshot(true);
                    lastSnapshotTime_ = now;
                }
            }
            
            // 检查键盘输入（用于检测活动）
            if (isKeyPressed()) {
                lastActivity_ = now;
            }
            
            std::this_thread::sleep_for(seconds(1));
        }
        
        watcher_->stop();
    }
    
    void shutdown() {
        running_ = false;
    }
    
    void takeSnapshot(bool autoSave, const std::string& message = "") {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        
        // TODO: 实现快照逻辑
        std::cout << "Taking snapshot: " << (autoSave ? "auto" : "manual") 
                  << " - " << message << std::endl;
        
        // 生成快照ID
        auto t = system_clock::to_time_t(system_clock::now());
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&t));
        std::string snapshotId = buf;
        
        // 创建快照对象
        Snapshot snapshot;
        snapshot.id = snapshotId;
        snapshot.timestamp = t;
        snapshot.autoSave = autoSave;
        snapshot.message = message.empty() ? generateAutoMessage() : message;
        
        // 存储快照
        storage_->store(snapshot);
        
        lastSnapshotTime_ = steady_clock::now();
    }
    
    bool restoreSnapshot(const std::string& snapshotId) {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        
        try {
            Snapshot snapshot = storage_->load(snapshotId);
            // TODO: 实现恢复逻辑
            std::cout << "Restoring snapshot: " << snapshotId << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to restore snapshot: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool undo() {
        std::vector<Snapshot> snapshots = storage_->list();
        if (snapshots.empty()) {
            std::cerr << "No snapshots available" << std::endl;
            return false;
        }
        
        // 获取最后一个快照
        Snapshot last = snapshots.back();
        
        // 恢复到上一个快照
        return restoreSnapshot(last.id);
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
        // TODO: 实现当前快照ID
        return storage_->lastSnapshotId();
    }
    
    void createTempBranch() {
        if (tempBranchActive_) {
            std::cerr << "Temp branch already active" << std::endl;
            return;
        }
        
        // TODO: 在内存中创建临时分支
        std::cout << "Creating temporary branch in memory" << std::endl;
        tempBranchActive_ = true;
    }
    
    void commitTempBranch(const std::string& name) {
        if (!tempBranchActive_) {
            std::cerr << "No active temp branch" << std::endl;
            return;
        }
        
        // TODO: 将临时分支保存为永久分支
        std::cout << "Committing temp branch as: " << name << std::endl;
        tempBranchActive_ = false;
    }
    
    void discardTempBranch() {
        if (!tempBranchActive_) {
            std::cerr << "No active temp branch" << std::endl;
            return;
        }
        
        // TODO: 丢弃临时分支
        std::cout << "Discarding temp branch" << std::endl;
        tempBranchActive_ = false;
    }

private:
    void loadIgnorePatterns() {
        fs::path confPath = workspace_ / ".clay" / "clay.conf";
        if (!fs::exists(confPath)) return;
        
        std::ifstream confFile(confPath);
        std::string line;
        
        while (std::getline(confFile, line)) {
            // 跳过注释和空行
            if (line.empty() || line[0] == '#' || line[0] == '[') 
                continue;
            
            // 解析键值对
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
                // 分割忽略模式
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
            // 简单模式匹配实现
            if (pattern.find('*') != std::string::npos) {
                // 通配符匹配
                std::string regexPattern = std::regex_replace(
                    pattern, 
                    std::regex("\\*"), 
                    ".*"
                );
                std::regex re(regexPattern, std::regex::icase);
                if (std::regex_match(relPath.string(), re)) {
                    return true;
                }
            } else {
                // 完全匹配
                if (relPath == pattern) {
                    return true;
                }
            }
        }
        return false;
    }
    
    std::string generateAutoMessage() const {
        // TODO: 生成更有意义的自动消息
        return "Auto snapshot";
    }
    
    std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), 
            [](unsigned char c){ return std::isspace(c); });
        auto end = std::find_if_not(str.rbegin(), str.rend(), 
            [](unsigned char c){ return std::isspace(c); }).base();
        return (start < end) ? std::string(start, end) : "";
    }
    
    bool isKeyPressed() {
#ifdef _WIN32
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) || 
               (GetAsyncKeyState(VK_CONTROL) & 0x8000) || 
               (GetAsyncKeyState(VK_MENU) & 0x8000);
#else
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        
        int ch = 0;
        int bytes = 0;
        ioctl(STDIN_FILENO, FIONREAD, &bytes);
        if (bytes > 0) {
            ch = getchar();
        }
        
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return bytes > 0;
#endif
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

// Core 单例实现
Core& Core::instance() {
    static Core instance;
    return instance;
}

Core::Core() : impl_(std::make_unique<Impl>()) {}
Core::~Core() = default;

bool Core::init(const std::string& workspace) { 
    return impl_->init(workspace); 
}
void Core::run() { impl_->run(); }
void Core::shutdown() { impl_->shutdown(); }
void Core::takeSnapshot(bool autoSave, const std::string& message) { 
    impl_->takeSnapshot(autoSave, message); 
}
bool Core::restoreSnapshot(const std::string& snapshotId) { 
    return impl_->restoreSnapshot(snapshotId); 
}
bool Core::undo() { return impl_->undo(); }
std::vector<std::string> Core::listSnapshots() { 
    return impl_->listSnapshots(); 
}
std::string Core::currentSnapshotId() const { 
    return impl_->currentSnapshotId(); 
}
void Core::createTempBranch() { impl_->createTempBranch(); }
void Core::commitTempBranch(const std::string& name) { 
    impl_->commitTempBranch(name); 
}
void Core::discardTempBranch() { impl_->discardTempBranch(); }

} // namespace clay