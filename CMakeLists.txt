cmake_minimum_required(VERSION 3.12)
project(clay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_executable(clay
    src/core.cpp
    src/snapshot.cpp
    src/storage.cpp
    src/watcher.cpp
    src/command.cpp
    src/daemon.cpp
    src/main.cpp
)

target_include_directories(clay PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    third_party
    third_party/bsdiff
)


add_library(bsdiff STATIC
    third_party/bsdiff/bsdiff.c
    third_party/bsdiff/bspatch.c
)
target_include_directories(bsdiff PUBLIC third_party/bsdiff)


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

    add_subdirectory(third_party/lz4/build/cmake)
    set(LZ4_TARGET lz4)
else()

    set(LZ4_TARGET lz4::lz4)
endif()


target_link_libraries(clay PRIVATE
    sqlite3
    ${LZ4_TARGET}
    bsdiff
)


install(TARGETS clay DESTINATION bin)