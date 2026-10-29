cmake_minimum_required(VERSION 3.16)
project(kindling VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(KINDLING_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(KINDLING_BUILD_TESTS "Build the test suite" ON)
option(KINDLING_WITH_STRATA "Build the playable game on the Strata engine" ON)
option(KINDLING_WEB "Build the browser game (Emscripten + Strata's WebGL 2 backend)" OFF)

# Emscripten: the rules and tests build as they are and the tests run under
# Node; KINDLING_WEB adds the browser game. Strata compiles C++ exceptions as
# wasm exceptions, and everything in one .wasm must match.
if(EMSCRIPTEN)
  cmake_minimum_required(VERSION 3.24)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fwasm-exceptions>")
  add_link_options(-fwasm-exceptions -sALLOW_MEMORY_GROWTH=1)
  if(KINDLING_WITH_STRATA)
    message(FATAL_ERROR "KINDLING_WITH_STRATA builds the desktop game: use -DKINDLING_WITH_STRATA=OFF -DKINDLING_WEB=ON")
  endif()
endif()
if(KINDLING_WEB AND NOT EMSCRIPTEN)
  message(FATAL_ERROR "KINDLING_WEB needs Emscripten (emcmake cmake ...)")
endif()

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

# The restoration as a usd-replay/1 replay, written by usd-recorder's vendored,
# dependency-free header (third_party/usd_recorder). No OpenUSD needed.
add_library(kindling_record STATIC src/record/replay.cpp)
target_include_directories(kindling_record PUBLIC src)
target_include_directories(kindling_record SYSTEM PRIVATE third_party)
target_link_libraries(kindling_record PUBLIC kindling_sim)
kindling_warnings(kindling_record)

# Saves: the World as a checksummed file, written atomically, with the previous
# save kept. Kindling's own format for the MVP (see ARCHITECTURE.md).
add_library(kindling_save STATIC src/save/save.cpp src/save/sha256.cpp)
target_include_directories(kindling_save PUBLIC src)
target_link_libraries(kindling_save PUBLIC kindling_sim)
kindling_warnings(kindling_save)

# The playtest probe (--playtest-out): state.json and events.jsonl for AI
# playtesters, in the format playtester's probe contract defines. No engine.
add_library(kindling_playtest STATIC src/playtest/probe.cpp)
target_include_directories(kindling_playtest PUBLIC src)
target_link_libraries(kindling_playtest PUBLIC kindling_sim)
target_compile_definitions(kindling_playtest PRIVATE KINDLING_VERSION="${PROJECT_VERSION}")
kindling_warnings(kindling_playtest)

if(KINDLING_BUILD_TESTS)
  enable_testing()
  foreach(t movement determinism targets geometry clean repair_furnish kindle playthrough saves playtest math grime settler)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE kindling_sim kindling_geometry kindling_record kindling_save kindling_playtest)
    target_include_directories(test_${t} PRIVATE tests)
    kindling_warnings(test_${t})
    if(EMSCRIPTEN)
      # Under Node, the tests read and write the real filesystem.
      target_link_options(test_${t} PRIVATE -sNODERAWFS=1)
    endif()
    add_test(NAME ${t} COMMAND test_${t} "${CMAKE_CURRENT_BINARY_DIR}")
  endforeach()
  # The frozen saves a released build wrote (tests/fixtures/README.md).
  target_compile_definitions(test_saves PRIVATE KINDLING_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
endif()

# ---------------------------------------------------------------- the game on Strata
# Strata is fetched at KINDLING_STRATA_REF (the v0.7.0 tag), or taken from a local checkout with
# -DFETCHCONTENT_SOURCE_DIR_STRATA=<path>. The desktop game (KINDLING_WITH_STRATA)
# and the browser game (KINDLING_WEB) share everything but their main().
if(KINDLING_WITH_STRATA OR KINDLING_WEB)
  cmake_minimum_required(VERSION 3.24)
  include(FetchContent)
  set(KINDLING_STRATA_REF "20827c3d67e71c1f942c29e7cf8adfa4801188a8" CACHE STRING "Strata git ref (a full commit SHA)")
  set(STRATA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  if(KINDLING_WEB)
    set(STRATA_WITH_USD OFF CACHE BOOL "" FORCE)
    set(STRATA_WITH_SIM OFF CACHE BOOL "" FORCE)
  endif()
  FetchContent_Declare(strata
    GIT_REPOSITORY https://github.com/Preponderous-Software/strata.git
    GIT_TAG ${KINDLING_STRATA_REF})
  FetchContent_MakeAvailable(strata)

  add_library(kindling_bridge STATIC src/bridge/scene.cpp)
  target_include_directories(kindling_bridge PUBLIC src)
  target_link_libraries(kindling_bridge PUBLIC kindling_geometry strata::static)
  kindling_warnings(kindling_bridge)

  # Playing: the HUD, the reveal, and a game session (input, autosave).
  add_library(kindling_play STATIC src/play/hud.cpp src/play/session.cpp)
  target_include_directories(kindling_play PUBLIC src)
  target_link_libraries(kindling_play PUBLIC kindling_bridge kindling_record kindling_save kindling_playtest)
  kindling_warnings(kindling_play)
endif()

if(KINDLING_WITH_STRATA)
  add_executable(kindling app/main.cpp)
  target_link_libraries(kindling PRIVATE kindling_play)
  kindling_warnings(kindling)
endif()

# emcmake cmake -S . -B build-web -DKINDLING_WITH_STRATA=OFF -DKINDLING_WEB=ON:
# the page is kindling.html + .js + .wasm; saves go to IndexedDB (docs/WEB.md).
if(KINDLING_WEB)
  add_executable(kindling-web app/web_main.cpp)
  target_link_libraries(kindling-web PRIVATE kindling_play)
  kindling_warnings(kindling-web)
  set_target_properties(kindling-web PROPERTIES OUTPUT_NAME kindling SUFFIX ".html")
  target_link_options(kindling-web PRIVATE
    -sSTACK_SIZE=1MB
    -lidbfs.js
    "-sEXPORTED_FUNCTIONS=_main,_kindling_look,_kindling_stick,_kindling_use,_kindling_press,_kindling_captured,_kindling_help,_kindling_skip_reveal,_kindling_save,_kindling_restart,_kindling_resize,_kindling_status"
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,FS,IDBFS,addRunDependency,removeRunDependency
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
  set_property(TARGET kindling-web APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
  # The loading shell (web/strata-kit, vendored from Strata) and its bridge go
  # inline into the page, with the download's size (web/strata_page.cmake).
  set_property(TARGET kindling-web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/web/strata-kit/strata-loader.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/strata-boot.js")
  add_custom_command(TARGET kindling-web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DHTML=$<TARGET_FILE:kindling-web>"
            "-DWASM=$<TARGET_FILE_DIR:kindling-web>/$<TARGET_FILE_BASE_NAME:kindling-web>.wasm"
            "-DWEB=${CMAKE_CURRENT_SOURCE_DIR}/web" -P "${CMAKE_CURRENT_SOURCE_DIR}/web/strata_page.cmake"
    VERBATIM)
endif()
