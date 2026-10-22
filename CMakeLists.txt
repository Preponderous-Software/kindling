cmake_minimum_required(VERSION 3.16)
project(kindling VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(KINDLING_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(KINDLING_BUILD_TESTS "Build the test suite" ON)
option(KINDLING_WITH_STRATA "Build the playable game on the Strata engine" ON)

function(kindling_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(KINDLING_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(KINDLING_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# The simulation: the world, the rules, targets. No engine, no GPU, so it
# builds and tests in seconds. Never fuse multiply-adds: the rules must give
# bit-identical results on every compiler.
add_library(kindling_sim STATIC
  src/sim/autopilot.cpp
  src/sim/brief.cpp
  src/sim/grime.cpp
  src/sim/items.cpp
  src/sim/layout.cpp
  src/sim/progress.cpp
  src/sim/rules.cpp
  src/sim/settler.cpp
  src/sim/target.cpp
  src/sim/world.cpp)
target_include_directories(kindling_sim PUBLIC src)
kindling_warnings(kindling_sim)
if(MSVC)
  target_compile_options(kindling_sim PRIVATE /fp:precise)
else()
  target_compile_options(kindling_sim PRIVATE -ffp-contract=off)
endif()

# The cabin and yard as triangle meshes, tagged with their targets. Still no
# engine: the render bridge below uploads them to Strata.
add_library(kindling_geometry STATIC
  src/bridge/geometry.cpp
  src/bridge/room.cpp)
target_include_directories(kindling_geometry PUBLIC src)
target_link_libraries(kindling_geometry PUBLIC kindling_sim)
kindling_warnings(kindling_geometry)

if(KINDLING_BUILD_TESTS)
  enable_testing()
  foreach(t movement determinism targets geometry clean repair_furnish kindle playthrough)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE kindling_sim kindling_geometry)
    target_include_directories(test_${t} PRIVATE tests)
    kindling_warnings(test_${t})
    add_test(NAME ${t} COMMAND test_${t})
  endforeach()
endif()

# ---------------------------------------------------------------- the game on Strata
# Strata is fetched at KINDLING_STRATA_REF, or taken from a local checkout with
# -DFETCHCONTENT_SOURCE_DIR_STRATA=<path>.
if(KINDLING_WITH_STRATA)
  cmake_minimum_required(VERSION 3.24)
  include(FetchContent)
  set(KINDLING_STRATA_REF "6d0eeb7b6e61dc525fc88821150f93ebfd14c5f6" CACHE STRING "Strata git ref (a full commit SHA)")
  set(STRATA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(strata
    GIT_REPOSITORY https://github.com/Preponderous-Software/strata.git
    GIT_TAG ${KINDLING_STRATA_REF})
  FetchContent_MakeAvailable(strata)

  add_library(kindling_bridge STATIC src/bridge/scene.cpp)
  target_include_directories(kindling_bridge PUBLIC src)
  target_link_libraries(kindling_bridge PUBLIC kindling_geometry strata::static)
  kindling_warnings(kindling_bridge)

  add_executable(kindling app/main.cpp)
  target_link_libraries(kindling PRIVATE kindling_bridge)
  kindling_warnings(kindling)
endif()
