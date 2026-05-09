cmake_minimum_required(VERSION 3.16)
project(cafs_benchmark CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-O3 -mavx2 -mbmi -DNDEBUG)

# CAFS headers (cafs.hpp, cafs2.hpp).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# competitors.hpp resolves third-party headers via paths relative to
# the benchmark directory, so add it to the include search list.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Optional: Google Highway brings in vqsort. Clone the Highway repo into
# third_party/highway_repo (see third_party/README.md). When present, the
# subdirectory CMake provides the hwy and hwy_contrib targets.
set(HIGHWAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/highway_repo)
if(EXISTS ${HIGHWAY_DIR}/CMakeLists.txt)
    set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory(${HIGHWAY_DIR} ${CMAKE_BINARY_DIR}/highway EXCLUDE_FROM_ALL)
endif()

add_executable(bench_grid main_bigdata.cpp)

if(TARGET hwy_contrib)
    target_link_libraries(bench_grid PRIVATE hwy hwy_contrib)
endif()
