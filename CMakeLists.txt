cmake_minimum_required(VERSION 3.16)

project(cachesimulator LANGUAGES CXX)

add_executable(cache_sim cache.cpp)
target_compile_features(cache_sim PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(cache_sim PRIVATE /W4 /permissive-)
else()
    target_compile_options(cache_sim PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if (MINGW)
    target_link_options(cache_sim PRIVATE -static)
endif()

enable_testing()

add_test(
    NAME cache_regression
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCACHE_EXE=$<TARGET_FILE:cache_sim>"
        "-DREFERENCE_CSV=${CMAKE_SOURCE_DIR}/results.csv"
        "-DOUTPUT_CSV=${CMAKE_BINARY_DIR}/test_results.csv"
        -P "${CMAKE_SOURCE_DIR}/tests/run_regression.cmake"
)
