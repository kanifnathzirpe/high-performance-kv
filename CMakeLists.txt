cmake_minimum_required(VERSION 3.20)

project(high_performance_kv
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(kv_store
    src/kv_store.cpp
    src/shard.cpp
)

target_include_directories(kv_store
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(googletest)

    add_executable(kv_store_test
        tests/kv_store_test.cpp
    )

    target_link_libraries(kv_store_test
        PRIVATE
            kv_store
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(kv_store_test)
endif()
