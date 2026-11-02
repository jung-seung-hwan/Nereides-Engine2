cmake_minimum_required(VERSION 3.20)
project(KrakenLearning LANGUAGES CXX)

add_executable(KrakenLearning
    src/main.cpp
    src/Application.cpp
    src/Application.h
)

set_target_properties(KrakenLearning PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(KrakenLearning PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(KrakenLearning PRIVATE -Wall -Wextra -Wpedantic)
endif()
