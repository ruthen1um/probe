cmake_minimum_required(VERSION 4.3.2)

project(probe LANGUAGES CXX)

find_package(nlohmann_json REQUIRED)
find_package(libmagic REQUIRED)
find_package(args REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_executable(${CMAKE_PROJECT_NAME})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libmagic::libmagic)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE taywee::args)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
