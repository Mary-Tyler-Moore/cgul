cmake_minimum_required(VERSION 3.22)
project(cgul LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CGUL_BUILD_SFML_DEMO "Build SFML demo app" ON)

add_library(cgul_core
  src/frame.cpp
  src/cgul_document.cpp
  src/validate.cpp
  src/layout_composer.cpp
  src/equality.cpp
)
target_include_directories(cgul_core PUBLIC include)
target_compile_features(cgul_core PUBLIC cxx_std_20)

if (MSVC)
  target_compile_options(cgul_core PRIVATE /W4 /WX)
else()
  target_compile_options(cgul_core PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(cgul_cli
  apps/cgul_cli/main.cpp
)
target_link_libraries(cgul_cli PRIVATE cgul_core)

add_executable(cgul_smoke
  apps/cgul_tests_smoke/main.cpp
)
target_link_libraries(cgul_smoke PRIVATE cgul_core)

if (CGUL_BUILD_SFML_DEMO)
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
endif()

enable_testing()
add_test(NAME cgul_smoke COMMAND cgul_smoke)
set_tests_properties(cgul_smoke PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
