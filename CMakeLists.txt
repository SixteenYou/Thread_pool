cmake_minimum_required(VERSION 3.16)
project(NetDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
	thread_pool 
	compare/BS_thread_pool.hpp 
	third_party/concurrentqueue.h 
	test.cpp 
	threadpool/thread_pool.cpp
)