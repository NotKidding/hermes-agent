cmake_minimum_required(VERSION 3.10)
project(HermesAgent)

set(CMAKE_CXX_STANDARD 17)

# Find libcurl (Essential for HTTP communication)
find_package(CURL REQUIRED)

include_directories(include)

# Define the executable and source files
add_executable(hermes src/main.cpp)

# Link the curl library to our agent
target_link_libraries(hermes PRIVATE CURL::libcurl)