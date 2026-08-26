cmake_minimum_required(VERSION 3.28)
project(FireStorm)

option(FIRESTORM_BUILD_APPLICATION "Build the FIRE STORM application" OFF)
option(FIRESTORM_BUILD_TESTING "Build the FIRE STORM tests" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ProjectFlags INTERFACE)

target_compile_options(ProjectFlags INTERFACE
        -Wall
        -Wextra
        -Wpedantic
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

target_link_options(ProjectFlags INTERFACE
        $<$<CONFIG:Release>:
        -Wl,--gc-sections
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
        -pie
        -s
        >
)

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
    target_link_libraries(FireStorm PRIVATE FireStormCore ProjectFlags)
endif ()

include(CTest)

if (FIRESTORM_BUILD_TESTING)
    add_library(FireStormTestCore
            src/tests/TestHarness.hpp
            src/tests/TestHarness.cpp
    )

    target_link_libraries(FireStormTestCore PRIVATE FireStormCore ProjectFlags)

    set(UNIT_TEST_EXECUTABLES Decoder)

    foreach (TEST_EXE IN LISTS UNIT_TEST_EXECUTABLES)
        set(TEST_EXE_NAME "Test${TEST_EXE}")
        add_executable(${TEST_EXE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/unit/${TEST_EXE_NAME}.cpp")
        target_link_libraries(${TEST_EXE_NAME} PRIVATE FireStormTestCore)

        add_test(
                NAME ${TEST_EXE}
                COMMAND ${TEST_EXE_NAME}
        )

        set_tests_properties(
                ${TEST_EXE}
                PROPERTIES
                TIMEOUT 5
        )
    endforeach ()
endif ()
