cmake_minimum_required(VERSION 3.23)

project(MarketOrderBookSimulator VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MDSIM_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
)

FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.4.tar.gz
)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
)
FetchContent_MakeAvailable(Catch2)
FetchContent_MakeAvailable(nlohmann_json)

add_library(mdsim_core STATIC
    src/decimal.cpp
    src/event_parser.cpp
    src/execution.cpp
    src/fee_model.cpp
    src/order_book.cpp
    src/reference_book.cpp
    src/report_writer.cpp
    src/replay_engine.cpp
    src/sequencer.cpp
)

target_include_directories(mdsim_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(mdsim_core PRIVATE ${MDSIM_WARNINGS})
target_link_libraries(mdsim_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(mdsim
    app/main.cpp
)

target_link_libraries(mdsim PRIVATE mdsim_core)
target_compile_options(mdsim PRIVATE ${MDSIM_WARNINGS})

if(BUILD_TESTING)
    add_executable(mdsim_tests
        tests/event_parser_test.cpp
        tests/execution_test.cpp
        tests/reference_book_test.cpp
        tests/replay_engine_test.cpp
        tests/order_book_test.cpp
        tests/sequencer_test.cpp
    )

    target_link_libraries(mdsim_tests
        PRIVATE
            mdsim_core
            Catch2::Catch2WithMain
    )

    target_compile_options(mdsim_tests PRIVATE ${MDSIM_WARNINGS})

    include(Catch)
    catch_discover_tests(mdsim_tests)
endif()
