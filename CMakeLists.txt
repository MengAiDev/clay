cmake_minimum_required(VERSION 3.12)
project(clay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展

# 添加可执行文件（移到前面）
add_executable(clay
    src/core.cpp
    src/snapshot.cpp
    src/storage.cpp
    src/watcher.cpp
    src/command.cpp
    src/daemon.cpp
    src/main.cpp
)

# 更现代的包含目录设置方式（替代 include_directories）
target_include_directories(clay PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    third_party
    third_party/bsdiff
)

# 添加bsdiff库（先于主目标定义）
add_library(bsdiff STATIC
    third_party/bsdiff/bsdiff.c
    third_party/bsdiff/bspatch.c
)
target_include_directories(bsdiff PUBLIC third_party/bsdiff)

# LZ4配置选项（优先使用系统安装的版本）
option(USE_SYSTEM_LZ4 "Use system-installed LZ4 library" ON)

if(USE_SYSTEM_LZ4)
    find_package(lz4 QUIET)
    if(lz4_FOUND)
        message(STATUS "Using system LZ4 library")
    else()
        message(STATUS "System LZ4 not found, using bundled version")
        set(USE_SYSTEM_LZ4 OFF)
    endif()
endif()

if(NOT USE_SYSTEM_LZ4)
    # 使用内嵌的LZ4
    add_subdirectory(third_party/lz4/build/cmake)
    set(LZ4_TARGET lz4)
else()
    # 使用系统的LZ4
    set(LZ4_TARGET lz4::lz4)
endif()

# 链接所有库
target_link_libraries(clay PRIVATE
    sqlite3
    ${LZ4_TARGET}
    bsdiff
)

# 安装
install(TARGETS clay DESTINATION bin)