cmake_minimum_required(VERSION 3.28)
project(FireStorm)

option(FIRESTORM_BUILD_APPLICATION "Build the FIRE STORM application" OFF)
option(FIRESTORM_BUILD_TESTING "Build the FIRE STORM tests" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO add compiler options
# -Wall
# -Wextra
# -Wpedantic
# $<$<CONFIG:Release>:
# -O3
# -DNDEBUG
# -fvisibility=hidden
# -fvisibility-inlines-hidden
# -fstack-protector-strong
# -ffunction-sections
# -fdata-sections
# >

# TODO add linker options
# $<$<CONFIG:Release>:
# -Wl,--gc-sections
# -Wl,-z,relro
# -Wl,-z,now
# -Wl,-z,noexecstack
# -pie
# -s
# >

add_library(FireStormCore
        src/TCPConnection.cpp
        src/TCPConnection.hpp
        src/CTFSPacket.cpp
        src/CTFSPacket.hpp
        src/TrafficDecoder.cpp
        src/TrafficDecoder.hpp
        src/Driver.cpp
        src/Driver.hpp
        src/AsyncFrameSink.cpp
        src/AsyncFrameSink.hpp
        src/EthernetFrameView.hpp
        src/PassthroughAsyncFrameRouter.cpp
        src/PassthroughAsyncFrameRouter.hpp
        src/CTFSRecordView.hpp
        src/CTFSRecordView.cpp
        src/Logger.cpp
        src/Logger.hpp
        src/PDFWriter.hpp
        src/PDFWriter.cpp
        src/ClassifiedAsyncFrameRouter.cpp
        src/ClassifiedAsyncFrameRouter.hpp
        src/IFrameRouter.hpp
)

if (FIRESTORM_BUILD_APPLICATION)
    add_executable(FireStorm src/app/FireStorm.cpp)
    target_link_libraries(FireStorm PRIVATE FireStormCore)
endif ()

if (FIRESTORM_BUILD_TESTING)
    include(CTest)
    add_executable(FireStormTests src/app/FireStorm.cpp) # TODO
    target_link_libraries(FireStormTests PRIVATE FireStormCore)
endif ()
