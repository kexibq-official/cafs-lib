cmake_minimum_required(VERSION 3.18)
project(cafs_sort_py CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /DNDEBUG)
else()
    add_compile_options(-O3 -mavx2 -mbmi -DNDEBUG)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_cafs MODULE src/cafs_py.cpp)
target_include_directories(_cafs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

if(MINGW)
    target_link_options(_cafs PRIVATE -static-libstdc++ -static-libgcc -static -lwinpthread)
endif()

install(TARGETS _cafs DESTINATION cafs)
