cmake_minimum_required(VERSION 3.5)
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
        src/Logger.cpp
        src/Logger.hpp
)

target_compile_options(FireStorm PRIVATE
        $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -fstack-protector-strong
        -ffunction-sections
        -fdata-sections
        >
)

target_link_options(FireStorm PRIVATE
        $<$<CONFIG:Release>:
        -Wl,--gc-sections
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
        -pie
        >
)
