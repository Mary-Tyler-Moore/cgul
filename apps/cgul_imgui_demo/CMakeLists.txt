include(FetchContent)

option(CGUL_IMGUI_DEMO_USE_SYSTEM_SDL2 "Use system SDL2 for cgul_imgui_demo" ON)
option(CGUL_IMGUI_DEMO_USE_SYSTEM_ZLIB "Use system zlib for cgul_imgui_demo" ON)
option(CGUL_IMGUI_DEMO_USE_SYSTEM_NLOHMANN_JSON "Use system nlohmann_json for cgul_imgui_demo" OFF)

if(CGUL_IMGUI_DEMO_USE_SYSTEM_SDL2)
  find_package(SDL2 CONFIG REQUIRED)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.11
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL2)

  if(NOT TARGET SDL2::SDL2)
    if(TARGET SDL2-static)
      add_library(SDL2::SDL2 ALIAS SDL2-static)
    elseif(TARGET SDL2)
      add_library(SDL2::SDL2 ALIAS SDL2)
    else()
      message(FATAL_ERROR "Unable to resolve SDL2 target for cgul_imgui_demo")
    endif()
  endif()
endif()

if(CGUL_IMGUI_DEMO_USE_SYSTEM_ZLIB)
  find_package(ZLIB REQUIRED)
else()
  FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(zlib)

  if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlibstatic)
      add_library(ZLIB::ZLIB ALIAS zlibstatic)
    elseif(TARGET zlib)
      add_library(ZLIB::ZLIB ALIAS zlib)
    else()
      message(FATAL_ERROR "Unable to resolve ZLIB target for cgul_imgui_demo")
    endif()
  endif()
endif()

if(CGUL_IMGUI_DEMO_USE_SYSTEM_NLOHMANN_JSON)
  find_package(nlohmann_json CONFIG REQUIRED)
else()
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

if(NOT TARGET imgui::imgui)
  add_library(cgul_imgui_demo_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  )
  target_include_directories(cgul_imgui_demo_imgui PUBLIC ${imgui_SOURCE_DIR})
  add_library(imgui::imgui ALIAS cgul_imgui_demo_imgui)
endif()

add_executable(cgul_imgui_demo
  main.cpp
  src/app/Paths.cpp
  src/app/DemoPersistence.cpp
  src/world/WorldState.cpp
  src/render/ArtRenderer.cpp
  src/render/CalmRenderer.cpp
  src/render/CgulUiRenderer.cpp
  src/ui/ChunkExporterPanel.cpp
  src/chunkexporter/tools/ChunkExporterTool.cpp
  src/chunkexporter/tiled/TiledMap.cpp
  src/chunkexporter/tiled/TiledLayerCodec.cpp
  src/chunkexporter/tiled/AtlasTexture.cpp
  src/imgui_backends/imgui_impl_sdl2.cpp
  src/imgui_backends/imgui_impl_sdlrenderer2.cpp
)

target_include_directories(cgul_imgui_demo
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cgul_imgui_demo
  PRIVATE
    cgul_core
    imgui::imgui
    SDL2::SDL2
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

if(TARGET SDL2::SDL2main)
  target_link_libraries(cgul_imgui_demo PRIVATE SDL2::SDL2main)
endif()

set_target_properties(cgul_imgui_demo PROPERTIES
  CXX_STANDARD ${CGUL_CXX_STANDARD}
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)
