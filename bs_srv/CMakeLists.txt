cmake_minimum_required(VERSION 2.8)
set( PROJECT_NAME bs_srv )

add_definitions(-std=c++11 -O3 -fopenmp -Wall)

file(GLOB HOST_SRC "*.cpp" "*.c" "./../common/*.cpp" "./../common/*.c")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../common)

add_executable(${PROJECT_NAME} ${HOST_SRC})
target_link_libraries(${PROJECT_NAME} -lpthread -ldl)

