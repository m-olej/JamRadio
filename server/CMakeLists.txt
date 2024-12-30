# Project structure
# src/main.cpp -> server loop
# include/* -> header files (classess, functions)
cmake_minimum_required(VERSION 3.10)

project(JamRadio VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

file(GLOB SOURCES "src/*.cpp" "include/*.cpp")

add_executable(JamRadio ${SOURCES})
