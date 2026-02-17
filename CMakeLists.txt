cmake_minimum_required(VERSION 3.22)
project(cgul LANGUAGES CXX)

include(FetchContent)

set(CGUL_CXX_STANDARD "20" CACHE STRING "C++ standard for CGUL targets (17 or 20)")
set_property(CACHE CGUL_CXX_STANDARD PROPERTY STRINGS 17 20)
if (NOT CGUL_CXX_STANDARD STREQUAL "17" AND NOT CGUL_CXX_STANDARD STREQUAL "20")
  message(FATAL_ERROR "CGUL_CXX_STANDARD must be 17 or 20 (got: ${CGUL_CXX_STANDARD})")
endif()

option(CGUL_BUILD_SFML_DEMO "Build SFML demo app" OFF)
option(CGUL_BUILD_IMGUI_DEMO "Build ImGui SDL2 demo app" OFF)

message(STATUS "CGUL_CXX_STANDARD: ${CGUL_CXX_STANDARD}")
message(STATUS "CGUL_BUILD_SFML_DEMO: ${CGUL_BUILD_SFML_DEMO}")
message(STATUS "CGUL_BUILD_IMGUI_DEMO: ${CGUL_BUILD_IMGUI_DEMO}")
if (CGUL_BUILD_SFML_DEMO)
  message(STATUS "SFML FetchContent: ENABLED (SFML will be fetched)")
else()
  message(STATUS "SFML FetchContent: DISABLED (SFML will not be fetched)")
endif()

set(CMAKE_CXX_STANDARD ${CGUL_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(cgul_core
  src/frame.cpp
  src/cgul_document.cpp
  src/validate.cpp
  src/layout_composer.cpp
  src/equality.cpp
)
target_include_directories(cgul_core PUBLIC include)
set_target_properties(cgul_core PROPERTIES
  CXX_STANDARD ${CGUL_CXX_STANDARD}
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

if (MSVC)
  target_compile_options(cgul_core PRIVATE /W4 /WX)
else()
  target_compile_options(cgul_core PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(cgul_cli
  apps/cgul_cli/main.cpp
)
target_link_libraries(cgul_cli PRIVATE cgul_core)
set_target_properties(cgul_cli PROPERTIES
  CXX_STANDARD ${CGUL_CXX_STANDARD}
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

add_executable(cgul_smoke
  apps/cgul_tests_smoke/main.cpp
)
target_link_libraries(cgul_smoke PRIVATE cgul_core)
set_target_properties(cgul_smoke PROPERTIES
  CXX_STANDARD ${CGUL_CXX_STANDARD}
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

if (CGUL_BUILD_SFML_DEMO)
  if (CGUL_CXX_STANDARD STREQUAL "17")
    message(FATAL_ERROR "cgul_demo_sfml requires C++20. Configure with -DCGUL_CXX_STANDARD=20 (and optionally keep CGUL_BUILD_SFML_DEMO=ON).")
  endif()

  set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    sfml
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(sfml)

  add_executable(cgul_demo_sfml
    apps/cgul_demo_sfml/main.cpp
    apps/cgul_demo_sfml/glyph_grid_renderer.cpp
  )
  target_link_libraries(cgul_demo_sfml PRIVATE cgul_core SFML::Graphics SFML::Window SFML::System)
  set_target_properties(cgul_demo_sfml PROPERTIES
    CXX_STANDARD ${CGUL_CXX_STANDARD}
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
  )
endif()

if (CGUL_BUILD_IMGUI_DEMO)
  add_subdirectory(apps/cgul_imgui_demo)
endif()

enable_testing()
add_test(NAME cgul_smoke COMMAND cgul_smoke)
set_tests_properties(cgul_smoke PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
