cmake_minimum_required(VERSION 3.1.0)

project(bitrader)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)

set(CMAKE_CXX_STANDARD 11)

include(FindOpenMP)
if (OPENMP_FOUND)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

if (APPLE)
set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/binance-cxx-api")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tgbot-cpp")

add_executable(${PROJECT_NAME} src/bitrader.cpp src/telegram.cpp src/telegram_bot.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${JSONCPP_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/binance-cxx-api/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tgbot-cpp/include)
target_link_libraries(${PROJECT_NAME} binance-cxx-api tgbot-cpp ${OpenMP_CXX_LIBRARIES})

