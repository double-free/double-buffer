cmake_minimum_required(VERSION 3.15)
project(double_buffer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main library target
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Unit tests configuration
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Test executable
    add_executable(${PROJECT_NAME}_tests
        tests/DoubleBufferTests.cpp
    )

    find_package(Threads REQUIRED)
    # apt install libgtest-dev for gtest support
    find_package(GTest REQUIRED)

    target_link_libraries(${PROJECT_NAME}_tests
        PRIVATE
        ${PROJECT_NAME}
        gtest
        gtest_main
    )
endif()