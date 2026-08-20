cmake_minimum_required(VERSION 4.0)
project(FireStorm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FireStorm
        src/TCPConnection.cpp
        src/TCPConnection.hpp
        src/CTFSPacket.cpp
        src/CTFSPacket.hpp
        src/TrafficDecoder.cpp
        src/TrafficDecoder.hpp
        src/Driver.cpp
        src/Driver.hpp
        src/LazyFrameSink.cpp
        src/LazyFrameSink.hpp
        src/EthernetFrameView.hpp
        src/FrameRouter.cpp
        src/FrameRouter.hpp
        src/CTFSRecordView.hpp
        src/CTFSRecordView.cpp
)
