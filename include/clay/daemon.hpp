#pragma once
#include <string>

namespace clay {

class Daemon {
public:
    static Daemon& instance();
    
    bool start(const std::string& workspace);
    bool stop();
    bool isRunning() const;
    
private:
    Daemon();
    ~Daemon() = default;
    
    void mainLoop();
    void handleClient(int fd);
    
    std::string pidPath_;
    std::string sockPath_;
    bool running_;
};

} // namespace clay