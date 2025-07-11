#include "clay/daemon.hpp"
#include "clay/command.hpp"
#include "clay/core.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <filesystem>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sstream>
#include <cstring> // 添加 memset 头文件

namespace fs = std::filesystem;

namespace clay {

Daemon& Daemon::instance() {
    static Daemon instance;
    return instance;
}

Daemon::Daemon() : pidPath_(), sockPath_(), running_(false) {}

bool Daemon::start(const std::string& workspace) {
    // 检查是否已运行
    if (isRunning()) {
        std::cerr << "Daemon is already running" << std::endl;
        return false;
    }

    // 创建守护进程
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return false;
    }

    if (pid > 0) { // 父进程退出
        return true;
    }

    // 子进程成为守护进程
    umask(0);
    setsid();

    // 关闭标准文件描述符
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // 设置工作目录
    if (chdir(workspace.c_str())) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    // 确保.clay目录存在
    fs::path clayDir = fs::path(workspace) / ".clay";
    if (!fs::exists(clayDir)) {
        if (!fs::create_directories(clayDir)) {
            perror("create_directories");
            exit(EXIT_FAILURE);
        }
    }

    // 创建PID文件
    pidPath_ = workspace + "/.clay/clay.pid";
    std::ofstream pidFile(pidPath_);
    if (!pidFile) {
        perror("pid file");
        exit(EXIT_FAILURE);
    }
    pidFile << getpid() << std::endl;
    pidFile.close();

    // 创建socket文件
    sockPath_ = workspace + "/.clay/clay.sock";

    // 初始化核心
    if (!Core::instance().init(workspace)) {
        std::cerr << "Core initialization failed" << std::endl;
        unlink(pidPath_.c_str());
        exit(EXIT_FAILURE);
    }

    // 启动守护进程主循环
    running_ = true;
    mainLoop();

    // 清理
    unlink(pidPath_.c_str());
    unlink(sockPath_.c_str());
    exit(EXIT_SUCCESS);
}

bool Daemon::stop() {
    if (!isRunning()) {
        std::cerr << "Daemon is not running" << std::endl;
        return false;
    }

    // 读取PID
    std::ifstream pidFile(pidPath_);
    if (!pidFile) {
        perror("pid file");
        return false;
    }

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    // 发送终止信号
    if (kill(pid, SIGTERM) < 0) {
        perror("kill");
        return false;
    }

    // 等待进程退出
    sleep(1);
    return !isRunning();
}

bool Daemon::isRunning() const {
    if (pidPath_.empty()) return false;
    
    std::ifstream pidFile(pidPath_);
    if (!pidFile) return false;
    
    pid_t pid;
    pidFile >> pid;
    pidFile.close();
    
    // 检查进程是否存在
    return kill(pid, 0) == 0;
}

void Daemon::mainLoop() {
    // 创建Unix域套接字
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return;
    }

    // 绑定套接字
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath_.c_str(), sizeof(addr.sun_path)-1);

    unlink(sockPath_.c_str()); // 移除旧socket

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return;
    }

    // 监听
    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return;
    }

    // 主循环
    while (running_) {
        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        // 处理客户端请求
        handleClient(client_fd);
        close(client_fd);
    }

    close(sockfd);
}

void Daemon::handleClient(int fd) {
    // 简单的协议：命令长度(4字节) + 命令
    uint32_t cmd_len;
    if (read(fd, &cmd_len, sizeof(cmd_len)) != sizeof(cmd_len)) {
        perror("read cmd_len");
        return;
    }

    cmd_len = ntohl(cmd_len);
    std::vector<char> buffer(cmd_len);
    if (read(fd, buffer.data(), cmd_len) != static_cast<ssize_t>(cmd_len)) {
        perror("read cmd");
        return;
    }

    std::string command(buffer.data(), cmd_len);
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }

    // 执行命令
    std::ostringstream result;
    clay::Command::execute(args, result);

    // 发送结果
    std::string result_str = result.str();
    uint32_t result_len = htonl(result_str.size());
    write(fd, &result_len, sizeof(result_len));
    write(fd, result_str.data(), result_str.size());
}

} // namespace clay
