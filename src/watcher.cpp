#include "clay/watcher.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <thread>
#include <iostream>
#include <atomic>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

namespace clay {

#ifdef _WIN32

class Watcher::Impl {
public:
    Impl(const std::string& path, 
         const std::vector<std::string>& ignorePatterns,
         EventCallback callback)
        : path_(path), 
          ignorePatterns_(ignorePatterns),
          callback_(callback),
          stop_(false) {}
    
    void run() {
        HANDLE dir = CreateFileA(
            path_.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );

        if (dir == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to open directory: " << GetLastError() << std::endl;
            return;
        }

        char buffer[1024];
        DWORD bytesReturned;
        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        while (!stop_) {
            if (ReadDirectoryChangesW(
                dir,
                buffer,
                sizeof(buffer),
                TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | 
                FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | 
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytesReturned,
                &overlapped,
                NULL
            )) {
                WaitForSingleObject(overlapped.hEvent, INFINITE);
                if (stop_) break;

                FILE_NOTIFY_INFORMATION* info = 
                    reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
                
                while (info) {
                    std::wstring wpath(info->FileName, 
                                       info->FileNameLength / sizeof(wchar_t));
                    std::string path(wpath.begin(), wpath.end());
                    fs::path fullPath = fs::path(path_) / path;
                    
                    bool isDir = (info->Action == FILE_ACTION_ADDED || 
                                 info->Action == FILE_ACTION_REMOVED || 
                                 info->Action == FILE_ACTION_RENAMED_OLD_NAME);
                    
                    // 检查是否应该忽略
                    bool ignore = false;
                    for (const auto& pattern : ignorePatterns_) {
                        if (std::regex_match(path, std::regex(pattern))) {
                            ignore = true;
                            break;
                        }
                    }
                    
                    if (!ignore) {
                        callback_(fullPath.string(), isDir);
                    }
                    
                    if (info->NextEntryOffset == 0) break;
                    info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                        reinterpret_cast<char*>(info) + info->NextEntryOffset);
                }
                ResetEvent(overlapped.hEvent);
            }
        }

        CloseHandle(dir);
        CloseHandle(overlapped.hEvent);
    }
    
    void stop() {
        stop_ = true;
    }

private:
    std::string path_;
    std::vector<std::string> ignorePatterns_;
    EventCallback callback_;
    std::atomic<bool> stop_;
};

#else

class Watcher::Impl {
public:
    Impl(const std::string& path, 
         const std::vector<std::string>& ignorePatterns,
         EventCallback callback)
        : path_(path), 
          ignorePatterns_(ignorePatterns),
          callback_(callback),
          inotify_fd_(-1),
          stop_(false) {}
    
    void run() {
        inotify_fd_ = inotify_init1(IN_NONBLOCK);
        if (inotify_fd_ < 0) {
            perror("inotify_init1");
            return;
        }

        int wd = inotify_add_watch(inotify_fd_, path_.c_str(), 
                                  IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        if (wd < 0) {
            perror("inotify_add_watch");
            close(inotify_fd_);
            return;
        }

        char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        
        while (!stop_) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(inotify_fd_, &fds);

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int ret = select(inotify_fd_ + 1, &fds, NULL, NULL, &timeout);
            if (ret < 0) {
                perror("select");
                break;
            } else if (ret == 0) {
                continue; // Timeout
            }

            ssize_t len = read(inotify_fd_, buffer, sizeof(buffer));
            if (len < 0) {
                if (errno == EINTR) continue;
                perror("read");
                break;
            }

            const struct inotify_event* event;
            for (char* ptr = buffer; ptr < buffer + len; 
                 ptr += sizeof(struct inotify_event) + event->len) {
                
                event = reinterpret_cast<const struct inotify_event*>(ptr);
                
                if (event->mask & IN_IGNORED) continue;
                
                std::string name(event->name);
                fs::path fullPath = fs::path(path_) / name;
                
                bool isDir = (event->mask & IN_ISDIR);
                
                // 检查是否应该忽略
                bool ignore = false;
                for (const auto& pattern : ignorePatterns_) {
                    if (std::regex_match(name, std::regex(pattern))) {
                        ignore = true;
                        break;
                    }
                }
                
                if (!ignore) {
                    callback_(fullPath.string(), isDir);
                }
            }
        }

        close(inotify_fd_);
    }
    
    void stop() {
        stop_ = true;
    }

private:
    std::string path_;
    std::vector<std::string> ignorePatterns_;
    EventCallback callback_;
    int inotify_fd_;
    std::atomic<bool> stop_;
};

#endif

Watcher::Watcher(const std::string& path, 
                 const std::vector<std::string>& ignorePatterns,
                 EventCallback callback)
    : impl_(std::make_unique<Impl>(path, ignorePatterns, callback)) {}

Watcher::~Watcher() = default;

void Watcher::start() {
    std::thread t([this] { impl_->run(); });
    t.detach();
}

void Watcher::stop() {
    impl_->stop();
}

} // namespace clay