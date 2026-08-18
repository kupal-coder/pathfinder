cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
project(gd-sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/**/*.cpp)
add_library(gd-sim STATIC ${SOURCES})
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

target_include_directories(gd-sim PUBLIC include)
target_compile_definitions(gd-sim PRIVATE _HAS_ITERATOR_DEBUGGING=0)
if(MSVC)
    target_compile_options(gd-sim PRIVATE /O2 /fp:precise $<$<CONFIG:Debug>:/Zi>)
else()
    target_compile_options(gd-sim PRIVATE
        -O3 -fno-math-errno -fno-trapping-math -fomit-frame-pointer
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-fno-rtti -fvisibility=hidden>
    )
endif()

add_executable(gd-sim-test ${SOURCES} test/test.cpp)
set_target_properties(gd-sim-test PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
target_include_directories(gd-sim-test PRIVATE include)
if(MSVC)
    target_compile_options(gd-sim-test PRIVATE /O2)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(gd-sim-regression ${SOURCES} test/regression.cpp)
    set_target_properties(gd-sim-regression PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_include_directories(gd-sim-regression PRIVATE include)
    add_test(NAME gd-sim-regression COMMAND gd-sim-regression)
endif()
