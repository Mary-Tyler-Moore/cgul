#include "render/CalmRenderer.hpp"

#include "world/WorldState.hpp"

#include "cgul/core/frame.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace cgul_demo {
namespace {

struct TileVisual {
    char glyph;
    const char* layerName;
    cgul::Rgba8 fg;
    cgul::Rgba8 bg;
};

struct TileLookup {
    bool hasTile;
    TileVisual visual;
};

const TileVisual kLayerVisuals[] = {
    {' ', "DeepWater", {70, 120, 190, 255}, {8, 24, 64, 255}},
    {' ', "ShallowWater", {100, 150, 210, 255}, {14, 40, 78, 255}},
    {'.', "SandAndShore", {244, 220, 140, 255}, {74, 56, 28, 255}},
    {'.', "Sand", {228, 204, 120, 255}, {60, 46, 24, 255}},
    {'o', "Rocks", {200, 200, 208, 255}, {40, 40, 44, 255}},
    {'T', "Trees", {130, 205, 120, 255}, {20, 48, 20, 255}},
    {'#', "Huts", {240, 170, 130, 255}, {72, 42, 28, 255}},
};

const tiled::TiledLayer* FindLayerByName(const tiled::TiledMap& map, const char* name) {
    for (std::vector<tiled::TiledLayer>::const_iterator it = map.layers.begin(); it != map.layers.end(); ++it) {
        if (it->name == name && it->isTileLayer && it->visible) {
            return &(*it);
        }
    }
    return nullptr;
}

TileLookup LookupTile(const tiled::TiledMap& map, int tileX, int tileY) {
    TileLookup lookup;
    lookup.hasTile = false;
    lookup.visual = {' ', "", {180, 180, 180, 255}, {18, 18, 22, 255}};

    if (tileX < 0 || tileY < 0 || tileX >= map.width || tileY >= map.height) {
        return lookup;
    }

    const size_t index = static_cast<size_t>(tileY * map.width + tileX);
    // Explicit priority, highest first: Huts > Trees > Rocks > SandAndShore > Sand > ShallowWater > DeepWater
    for (size_t i = sizeof(kLayerVisuals) / sizeof(kLayerVisuals[0]); i > 0; --i) {
        const TileVisual& visual = kLayerVisuals[i - 1];
        const tiled::TiledLayer* layer = FindLayerByName(map, visual.layerName);
        if (!layer || index >= layer->gids.size() || layer->gids[index] == 0) {
            continue;
        }

        lookup.hasTile = true;
        lookup.visual = visual;
        return lookup;
    }

    return lookup;
}

void OverlayText(cgul::Frame* frame, int row, const std::string& text) {
    if (!frame || row < 0 || row >= frame->height) {
        return;
    }

    const int maxCols = std::min(frame->width, static_cast<int>(text.size()));
    for (int x = 0; x < maxCols; ++x) {
        cgul::Cell& cell = frame->at(x, row);
        cell.glyph = static_cast<unsigned char>(text[static_cast<size_t>(x)]);
        cell.fg = {230, 230, 235, 255};
        cell.bg = {24, 24, 28, 255};
    }
}

std::string FrameToAscii(const cgul::Frame& frame) {
    std::string output;
    const size_t reserveSize = static_cast<size_t>(frame.width * frame.height + frame.height);
    output.reserve(reserveSize);

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const char32_t glyph = frame.at(x, y).glyph;
            if (glyph >= 32 && glyph <= 126) {
                output.push_back(static_cast<char>(glyph));
            } else {
                output.push_back('?');
            }
        }
        output.push_back('\n');
    }

    return output;
}

}  // namespace

void CalmRenderer::Draw(WorldState* worldState) {
    if (!worldState) {
        ImGui::TextUnformatted("Renderer state unavailable.");
        return;
    }

    ImGui::BeginChild("CalmViewportCanvas", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (!worldState->hasMap || worldState->map.width <= 0 || worldState->map.height <= 0) {
        worldState->viewportHovered = ImGui::IsWindowHovered();
        worldState->viewportWidthPx = 0.0f;
        worldState->viewportHeightPx = 0.0f;
        worldState->visibleTileSpanX = 1.0f;
        worldState->visibleTileSpanY = 1.0f;
        ImGui::TextUnformatted("Load a Tiled JSON map to view CALM mode.");
        ImGui::EndChild();
        return;
    }

    worldState->ClampCameraToMap();

    const float mapWidth = static_cast<float>(worldState->map.width);
    const float mapHeight = static_cast<float>(worldState->map.height);
    const float visibleW = std::max(1.0f, mapWidth / worldState->zoom);
    const float visibleH = std::max(1.0f, mapHeight / worldState->zoom);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float charW = std::max(1.0f, ImGui::CalcTextSize("M").x);
    const float lineH = std::max(1.0f, ImGui::GetTextLineHeightWithSpacing());

    int gridW = static_cast<int>(available.x / charW);
    int gridH = static_cast<int>(available.y / lineH);
    if (gridW < 24) {
        gridW = 24;
    }
    if (gridH < 8) {
        gridH = 8;
    }

    gridW = std::min(gridW, 220);
    gridH = std::min(gridH, 120);

    cgul::Frame frame(gridW, gridH);
    frame.clear(U' ');

    for (int y = 0; y < gridH; ++y) {
        for (int x = 0; x < gridW; ++x) {
            const float tx = worldState->cameraTileX + (visibleW * (static_cast<float>(x) + 0.5f) / static_cast<float>(gridW));
            const float ty = worldState->cameraTileY + (visibleH * (static_cast<float>(y) + 0.5f) / static_cast<float>(gridH));
            const int mapX = std::max(0, std::min(static_cast<int>(tx), worldState->map.width - 1));
            const int mapY = std::max(0, std::min(static_cast<int>(ty), worldState->map.height - 1));

            const TileLookup lookup = LookupTile(worldState->map, mapX, mapY);
            cgul::Cell& cell = frame.at(x, y);
            if (lookup.hasTile) {
                cell.glyph = static_cast<unsigned char>(lookup.visual.glyph);
                cell.fg = lookup.visual.fg;
                cell.bg = lookup.visual.bg;
            } else {
                cell.glyph = ' ';
                cell.fg = {120, 120, 120, 255};
                cell.bg = {15, 15, 18, 255};
            }
        }
    }

    char line0[160];
    std::snprintf(line0, sizeof(line0), "MODE: CALM (TAB)");
    OverlayText(&frame, 0, line0);

    char line1[160];
    std::snprintf(line1, sizeof(line1), "Map %dx%d  tile %dx%d", worldState->map.width, worldState->map.height,
        worldState->map.tileWidth, worldState->map.tileHeight);
    OverlayText(&frame, 1, line1);

    char line2[160];
    std::snprintf(line2, sizeof(line2), "Camera %.1f, %.1f  zoom %.2f", worldState->cameraTileX,
        worldState->cameraTileY, worldState->zoom);
    OverlayText(&frame, 2, line2);

    char line3[200];
    if (worldState->hoverTileX >= 0 && worldState->hoverTileY >= 0) {
        const TileLookup hover = LookupTile(worldState->map, worldState->hoverTileX, worldState->hoverTileY);
        std::snprintf(line3, sizeof(line3), "Hover %d,%d  layer %s", worldState->hoverTileX,
            worldState->hoverTileY, hover.hasTile ? hover.visual.layerName : "none");
        worldState->hoverLayerName = hover.hasTile ? hover.visual.layerName : std::string();
    } else {
        std::snprintf(line3, sizeof(line3), "Hover -, -  layer none");
    }
    OverlayText(&frame, 3, line3);

    const std::string text = FrameToAscii(frame);

    const ImVec2 textOrigin = ImGui::GetCursorScreenPos();
    const float textW = static_cast<float>(gridW) * charW;
    const float textH = static_cast<float>(gridH) * lineH;

    worldState->viewportScreenX = textOrigin.x;
    worldState->viewportScreenY = textOrigin.y;
    worldState->viewportWidthPx = textW;
    worldState->viewportHeightPx = textH;
    worldState->visibleTileSpanX = visibleW;
    worldState->visibleTileSpanY = visibleH;
    worldState->viewportHovered = ImGui::IsWindowHovered();

    ImGui::TextUnformatted(text.c_str());

    if (worldState->viewportHovered && textW > 0.0f && textH > 0.0f) {
        const ImVec2 mousePos = ImGui::GetMousePos();
        const float relX = (mousePos.x - textOrigin.x) / textW;
        const float relY = (mousePos.y - textOrigin.y) / textH;
        if (relX >= 0.0f && relX <= 1.0f && relY >= 0.0f && relY <= 1.0f) {
            int hoverX = static_cast<int>(std::floor(worldState->cameraTileX + relX * visibleW));
            int hoverY = static_cast<int>(std::floor(worldState->cameraTileY + relY * visibleH));
            hoverX = std::max(0, std::min(hoverX, worldState->map.width - 1));
            hoverY = std::max(0, std::min(hoverY, worldState->map.height - 1));
            worldState->hoverTileX = hoverX;
            worldState->hoverTileY = hoverY;

            const TileLookup hover = LookupTile(worldState->map, hoverX, hoverY);
            worldState->hoverLayerName = hover.hasTile ? hover.visual.layerName : std::string();

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                worldState->selectedTileX = hoverX;
                worldState->selectedTileY = hoverY;
            }
        }
    }

    ImGui::EndChild();
}

}  // namespace cgul_demo
