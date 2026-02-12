cmake_minimum_required(VERSION 3.22)
project(cgul LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(cgul_core
  src/frame.cpp
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
