cmake_minimum_required(VERSION 3.16)

project(Chip8Emulator LANGUAGES CXX)

find_package(SDL3 REQUIRED)

add_executable(app
    src/main.cpp
    src/chip8.cpp
)

target_compile_features(app PRIVATE cxx_std_20)

target_link_libraries(app SDL3::SDL3)
