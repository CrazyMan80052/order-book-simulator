cmake_minimum_required(VERSION 3.23)

project(MarketOrderBookSimulator VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
FetchContent_MakeAvailable(Catch2)

add_library(mdsim_core STATIC
    src/decimal.cpp
    src/order_book.cpp
)

target_include_directories(mdsim_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(mdsim_core PRIVATE ${MDSIM_WARNINGS})

add_executable(mdsim
    app/main.cpp
)

target_link_libraries(mdsim PRIVATE mdsim_core)
target_compile_options(mdsim PRIVATE ${MDSIM_WARNINGS})

if(BUILD_TESTING)
    add_executable(mdsim_tests
        tests/order_book_test.cpp
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
