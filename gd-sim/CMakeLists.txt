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

# ---------------------------------------------------------------------------
# Test and benchmark tools.
#
# These build without the Geode SDK and without the game, so CI can run the
# whole physics and search suite on a plain Linux runner. They are only added
# when gd-sim is configured directly (not as a subdirectory of the mod), so a
# normal mod build stays lean.
# ---------------------------------------------------------------------------
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_package(Threads REQUIRED)

    # Golden-file physics regression runner.
    add_executable(gd-sim-golden ${SOURCES} test/golden.cpp)
    target_include_directories(gd-sim-golden PRIVATE include test)
    set_target_properties(gd-sim-golden PROPERTIES INTERPROCEDURAL_OPTIMIZATION 0)

    # Search engine correctness tests.
    add_executable(gd-sim-searchtest ${SOURCES} test/searchtest.cpp)
    target_include_directories(gd-sim-searchtest PRIVATE include test ../src)
    target_link_libraries(gd-sim-searchtest PRIVATE Threads::Threads)
    set_target_properties(gd-sim-searchtest PROPERTIES INTERPROCEDURAL_OPTIMIZATION 0)

    # Search benchmark / gauntlet.
    add_executable(gd-sim-searchbench ${SOURCES} test/searchbench.cpp)
    target_include_directories(gd-sim-searchbench PRIVATE include test ../src)
    target_link_libraries(gd-sim-searchbench PRIVATE Threads::Threads)

    enable_testing()
    add_test(NAME physics-golden COMMAND gd-sim-golden verify)
    add_test(NAME search-correctness COMMAND gd-sim-searchtest)
endif()
