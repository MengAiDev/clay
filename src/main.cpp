#include "clay/command.hpp"
#include "clay/daemon.hpp"
#include <vector>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <filesystem>

namespace fs = std::filesystem;

int sendCommandToDaemon(const std::vector<std::string>& args) {
    // 查找守护进程socket
    std::string sockPath = ".clay/clay.sock";
    if (!fs::exists(sockPath)) {
        // 尝试在父目录中查找
        for (int i = 0; i < 5; i++) {
            sockPath = "../" + sockPath;
            if (fs::exists(sockPath)) break;
        }
        if (!fs::exists(sockPath)) {
            std::cerr << "Clay daemon is not running. Use 'clay init' to start." << std::endl;
            return 1;
        }
    }

    // 连接到守护进程
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path)-1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    // 构造命令
    std::ostringstream oss;
    for (const auto& arg : args) {
        oss << arg << " ";
    }
    std::string command = oss.str();
    uint32_t cmd_len = htonl(command.size());

    // 发送命令
    if (write(sockfd, &cmd_len, sizeof(cmd_len)) != sizeof(cmd_len)) {
        perror("write cmd_len");
        close(sockfd);
        return 1;
    }
    
    if (write(sockfd, command.data(), command.size()) != static_cast<ssize_t>(command.size())) {
        perror("write cmd");
        close(sockfd);
        return 1;
    }

    // 接收结果
    uint32_t result_len;
    if (read(sockfd, &result_len, sizeof(result_len)) != sizeof(result_len)) {
        perror("read result_len");
        close(sockfd);
        return 1;
    }

    result_len = ntohl(result_len);
    std::vector<char> buffer(result_len);
    ssize_t bytes_read = read(sockfd, buffer.data(), result_len);
    if (bytes_read < 0) {
        perror("read result");
        close(sockfd);
        return 1;
    }
    
    if (bytes_read != static_cast<ssize_t>(result_len)) {
        std::cerr << "Incomplete result: " << bytes_read << "/" << result_len << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << std::string(buffer.data(), result_len);
    close(sockfd);
    return 0;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    if (args.empty()) {
        clay::Command::help(std::cout);
        return 1;
    }

    // 特殊命令直接处理
    if (args[0] == "init") {
        if (args.size() < 2) args.push_back(".");
        if (!clay::Daemon::instance().start(args[1])) {
            return 1;
        }
        std::cout << "Clay daemon started in " << args[1] << std::endl;
        return 0;
    } else if (args[0] == "stop") {
        if (clay::Daemon::instance().stop()) {
            std::cout << "Clay daemon stopped" << std::endl;
            return 0;
        }
        return 1;
    } else if (args[0] == "status" && args.size() == 1) {
        // 状态命令需要特殊处理以显示守护进程状态
        std::cout << "Clay daemon: "
                  << (clay::Daemon::instance().isRunning() ? "running" : "stopped")
                  << std::endl;
        return 0;
    }

    // 其他命令发送到守护进程
    return sendCommandToDaemon(args);
}