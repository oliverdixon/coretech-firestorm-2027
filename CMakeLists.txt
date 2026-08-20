cmake_minimum_required(VERSION 4.0)
project(FireStorm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FireStorm
        src/TCPConnection.cpp
        src/TCPConnection.hpp
        src/CTFSPacket.cpp
        src/CTFSPacket.hpp
        src/PacketFilter.cpp
        src/PacketFilter.hpp
        src/Driver.cpp
        src/Driver.hpp
)
