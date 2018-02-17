cmake_minimum_required(VERSION 3.1.0)

project(bitrader)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)

set(CMAKE_CXX_STANDARD 11)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/binance-cxx-api/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tgbot-cpp/include)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/binance-cxx-api)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/tgbot-cpp)

# Manual build type selection (for debugging purposes)
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

add_subdirectory(binance-cxx-api)
add_subdirectory(tgbot-cpp)

add_executable(bitrader bitrader.cpp)
target_link_libraries(bitrader binance-cxx-api tgbot-cpp)

