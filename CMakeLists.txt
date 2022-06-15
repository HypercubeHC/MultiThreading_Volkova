cmake_minimum_required(VERSION 3.22)
project(example)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

add_executable(multithreading main.cpp)
