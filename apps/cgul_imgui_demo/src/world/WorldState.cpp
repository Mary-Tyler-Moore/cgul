#include "world/WorldState.hpp"

#include <algorithm>

namespace cgul_demo {

void WorldState::ClampZoom() {
    const float kMinZoom = 1.0f;
    const float kMaxZoom = 12.0f;
    if (zoom < kMinZoom) {
        zoom = kMinZoom;
    }
    if (zoom > kMaxZoom) {
        zoom = kMaxZoom;
    }
}

void WorldState::ClampCameraToMap() {
    if (!hasMap || map.width <= 0 || map.height <= 0) {
        cameraTileX = 0.0f;
        cameraTileY = 0.0f;
        return;
    }

    ClampZoom();

    const float visibleW = std::max(1.0f, static_cast<float>(map.width) / zoom);
    const float visibleH = std::max(1.0f, static_cast<float>(map.height) / zoom);

    float maxX = static_cast<float>(map.width) - visibleW;
    float maxY = static_cast<float>(map.height) - visibleH;
    if (maxX < 0.0f) {
        maxX = 0.0f;
    }
    if (maxY < 0.0f) {
        maxY = 0.0f;
    }

    cameraTileX = std::max(0.0f, std::min(cameraTileX, maxX));
    cameraTileY = std::max(0.0f, std::min(cameraTileY, maxY));
}

void WorldState::ResetHover() {
    hoverTileX = -1;
    hoverTileY = -1;
    hoverLayerName.clear();
}

}  // namespace cgul_demo
