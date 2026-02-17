#pragma once

#include "chunkexporter/tiled/TiledMap.hpp"

#include <string>

namespace cgul_demo {

struct WorldState {
    bool calmMode = false;

    float cameraTileX = 0.0f;
    float cameraTileY = 0.0f;
    float zoom = 1.0f;

    int hoverTileX = -1;
    int hoverTileY = -1;
    int selectedTileX = -1;
    int selectedTileY = -1;
    std::string hoverLayerName;

    bool hasMap = false;
    tiled::TiledMap map;

    std::string statusText;
    std::string errorText;

    float viewportScreenX = 0.0f;
    float viewportScreenY = 0.0f;
    float viewportWidthPx = 0.0f;
    float viewportHeightPx = 0.0f;
    float visibleTileSpanX = 1.0f;
    float visibleTileSpanY = 1.0f;
    bool viewportHovered = false;

    void ClampZoom();
    void ClampCameraToMap();
    void ResetHover();
};

}  // namespace cgul_demo
