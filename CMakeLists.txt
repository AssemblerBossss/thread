cmake_minimum_required(VERSION 4.0)
project(untitled)

set(CMAKE_CXX_STANDARD 20)

add_executable(untitled main.cpp
        Timer.h)

add_executable(pz_1 pz_1/pz_1.cpp)


add_executable(pz_2 pz_2/pz_2.cpp)

add_executable(pz_3 pz_3/pz_3.cpp)

add_executable(pz_4 pz_4/pz_4.cpp)
add_executable(pz_4_call_once pz_4/call_once.cpp)
add_executable(pz_4_thread_local pz_4/thread_local.cpp)

add_executable(pz_5 pz_5/pz_5.cpp)
add_executable(pz_6 pz_6/PZ6_Decoder.cpp)
add_executable(pz_7 pz_7/pz_7.cpp)
