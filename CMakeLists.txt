cmake_minimum_required(VERSION 2.6)
project(ndl)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories(..)
add_executable(ndl unitTests/unitTests.cpp)
