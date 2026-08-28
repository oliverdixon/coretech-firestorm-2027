cmake_minimum_required(VERSION 3.28)
project(FireStorm)

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

add_executable(FireStorm src/app/FireStorm.cpp)
target_link_libraries(FireStorm PRIVATE FireStormCore ProjectFlags)

include(CTest)

if (FIRESTORM_BUILD_TESTING)
    #
    # Configure unit tests.
    #

    add_library(FireStormTestCore
            src/tests/TestHarness.hpp
            src/tests/TestHarness.cpp
    )

    set(TestTrafficDecoder_CASES
            decode_valid_packet
            accept_empty_input
            reject_partial_header
            reject_bad_checksum
            decode_segmented_packets
            reject_nonempty_truncated_packets
            decode_coalesced_records
            reject_nonzero_padding
            reject_incorrect_advertised_length
            decoder_can_be_reused
            decoder_can_recover
            compaction_preserves_partial
    )

    set(TestPDFWriter_CASES
            write_single_packet_exactly
            write_across_packet_boundaries
            accept_trailing_padding
            enforce_eof_search_limit
            reject_empty_input
            reject_invalid_header
            reject_invalid_tail
            ignore_duplicate_packets
            propagate_output_failures
    )

    target_link_libraries(FireStormTestCore PRIVATE FireStormCore ProjectFlags)
    file(GLOB UNIT_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/unit/*.cpp")

    # Each CPP file in src/tests/unit denotes a fixture of unit tests. An executable CMake target is produced for each
    # fixture. A fixture is composed of individual unit test cases, which are defined by the CMake list variable
    # Test{FIXTURE}_CASES.
    #
    # Each test case is registered with CTest under the name Unit.{FIXTURE}.{CASE} and associated with the labels
    # "Unit" and {FIXTURE}.

    foreach (TEST_PATH IN LISTS UNIT_TESTS)
        cmake_path(GET TEST_PATH STEM TEST_NAME)
        add_executable(${TEST_NAME} ${TEST_PATH})
        target_link_libraries(${TEST_NAME} PRIVATE FireStormTestCore)

        string(REGEX REPLACE "^Test" "" FIXTURE_NAME "${TEST_NAME}")
        foreach (CASE_NAME IN LISTS "${TEST_NAME}_CASES")
            set(CTEST_NAME "Unit.${FIXTURE_NAME}.${CASE_NAME}")

            add_test(
                    NAME "${CTEST_NAME}"
                    COMMAND
                    "$<TARGET_FILE:${TEST_NAME}>"
                    "--test"
                    "${CASE_NAME}"
            )

            set_tests_properties(
                    "${CTEST_NAME}"
                    PROPERTIES
                    LABELS "Unit;${FIXTURE_NAME}"
            )
        endforeach ()
    endforeach ()

    #
    # Configure CoreTech integration tests using the Python helper.
    #

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    function(add_integration_test)
        set(options)
        set(one_value_args NAME SCRIPT)
        set(multi_value_args ARGS)

        cmake_parse_arguments(
                TEST
                "${options}"
                "${one_value_args}"
                "${multi_value_args}"
                ${ARGN}
        )

        set(CTEST_NAME "Integration.${TEST_NAME}")

        add_test(
                NAME "${CTEST_NAME}"
                COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/coretech/run_integration.py"
                "--streamer"
                "${TEST_SCRIPT}"
                "--app"
                "$<TARGET_FILE:FireStorm>"
                ${TEST_ARGS}
        )

        set_tests_properties(
                "Integration.${TEST_NAME}"
                PROPERTIES
                TIMEOUT 60
                LABELS "Integration"
        )
    endfunction()

    add_integration_test(
            NAME Stage1
            SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/coretech/stage1/test.py"
    )

    add_integration_test(
            NAME Stage2
            SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/coretech/stage2/test.py"
            ARGS
            "--"
            "--classified"
    )
endif ()
