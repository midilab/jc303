cmake_minimum_required(VERSION 3.15)

# Standalone: cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(JC303Tests CXX)
    enable_testing()
endif()

add_executable(lfo_test lfo_test.cpp ../src/dsp/open303/dfl_LFO.cpp)
target_compile_features(lfo_test PRIVATE cxx_std_17)
target_include_directories(lfo_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303)
add_test(NAME lfo_test COMMAND lfo_test)
