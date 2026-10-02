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

# Open303 core tests need the CPM-fetched math_approx headers (root build: lib/math_approx-src).
# Standalone: cmake -S tests -B build-tests -DMATH_APPROX_INCLUDE=/path/to/math_approx-src/include
set(MATH_APPROX_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../lib/math_approx-src/include" CACHE PATH "math_approx include dir")
if(EXISTS "${MATH_APPROX_INCLUDE}/math_approx/math_approx.hpp")
    file(GLOB OPEN303_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303/*.cpp)
    add_executable(open303_lfo_test open303_lfo_test.cpp ${OPEN303_SOURCES})
    target_compile_features(open303_lfo_test PRIVATE cxx_std_17)
    target_include_directories(open303_lfo_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303
        ${MATH_APPROX_INCLUDE})
    add_test(NAME open303_lfo_test COMMAND open303_lfo_test)
endif()
