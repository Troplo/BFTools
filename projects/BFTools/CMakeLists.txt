cmake_minimum_required(VERSION 3.25)
project(BFTools LANGUAGES CXX)

add_executable(BFTools src/main.cpp)
target_link_libraries(BFTools PRIVATE Nomad::Archives)
target_compile_features(BFTools PRIVATE cxx_std_23)

