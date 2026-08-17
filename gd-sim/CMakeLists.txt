cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(gd-sim VERSION 0.1.0 LANGUAGES CXX)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/**/*.cpp)
add_library(gd-sim STATIC ${SOURCES})

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_options(gd-sim PRIVATE $<$<CONFIG:Debug>:-g> -O3 -fno-math-errno -fno-trapping-math -fomit-frame-pointer $<$<CONFIG:Release>:-fno-rtti -fvisibility=hidden>)


target_include_directories(gd-sim PUBLIC include)

target_compile_definitions(gd-sim PRIVATE -D_HAS_ITERATOR_DEBUGGING=0)

add_executable(gd-sim-test ${SOURCES} test/test.cpp)

set_target_properties(gd-sim-test PROPERTIES INTERPROCEDURAL_OPTIMIZATION 0)
target_include_directories(gd-sim-test PRIVATE include)

include(CTest)
if (BUILD_TESTING)
    add_executable(gd-sim-geometry-test src/util.cpp test/geometry.cpp)
    target_include_directories(gd-sim-geometry-test PRIVATE include)
    add_test(NAME gd-sim-geometry COMMAND gd-sim-geometry-test)

    add_executable(gd-sim-input-scheduler-test test/input_scheduler.cpp)
    target_include_directories(gd-sim-input-scheduler-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    add_test(NAME gd-sim-input-scheduler COMMAND gd-sim-input-scheduler-test)

    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_Interpreter_FOUND)
        add_test(
            NAME pathfinder-acceptance-fixtures
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate_test_levels.py
        )
    endif()
endif()