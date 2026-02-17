#include "render/ArtRenderer.hpp"

#include "chunkexporter/tools/ChunkExporterTool.hpp"
#include "world/WorldState.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace cgul_demo {
namespace {

std::string FindTopLayerName(const tiled::TiledMap& map, int tileX, int tileY) {
    if (tileX < 0 || tileY < 0 || tileX >= map.width || tileY >= map.height) {
        return std::string();
    }

    const size_t index = static_cast<size_t>(tileY * map.width + tileX);
    for (std::vector<tiled::TiledLayer>::const_reverse_iterator it = map.layers.rbegin();
         it != map.layers.rend(); ++it) {
        if (!it->isTileLayer || !it->visible) {
            continue;
        }
        if (index >= it->gids.size()) {
            continue;
        }
        if (it->gids[index] != 0) {
            return it->name;
        }
    }

    return std::string();
}

}  // namespace

void ArtRenderer::Draw(WorldState* worldState, tools::ChunkExporterTool* tool) {
    if (!worldState || !tool) {
        ImGui::TextUnformatted("Renderer state unavailable.");
        return;
    }

    ImGui::BeginChild("ArtViewportCanvas", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("MODE: ART (TAB)");

    if (!worldState->hasMap || worldState->map.width <= 0 || worldState->map.height <= 0) {
        worldState->viewportHovered = ImGui::IsWindowHovered();
        worldState->viewportWidthPx = 0.0f;
        worldState->viewportHeightPx = 0.0f;
        worldState->visibleTileSpanX = 1.0f;
        worldState->visibleTileSpanY = 1.0f;
        ImGui::TextUnformatted("Load a Tiled JSON map to preview.");
        ImGui::EndChild();
        return;
    }

    tool->EnsurePreviewReady();
    SDL_Texture* previewTexture = tool->GetPreviewTexture();
    if (!previewTexture) {
        worldState->viewportHovered = ImGui::IsWindowHovered();
        ImGui::TextUnformatted("Tile preview texture unavailable.");
        ImGui::EndChild();
        return;
    }

    worldState->ClampCameraToMap();

    const float mapWidth = static_cast<float>(worldState->map.width);
    const float mapHeight = static_cast<float>(worldState->map.height);
    const float visibleW = std::max(1.0f, mapWidth / worldState->zoom);
    const float visibleH = std::max(1.0f, mapHeight / worldState->zoom);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x < 2.0f || available.y < 2.0f) {
        ImGui::EndChild();
        return;
    }

    const float aspect = visibleH > 0.0f ? (visibleW / visibleH) : 1.0f;
    float drawWidth = available.x;
    float drawHeight = drawWidth / aspect;
    if (drawHeight > available.y) {
        drawHeight = available.y;
        drawWidth = drawHeight * aspect;
    }

    if (drawWidth < 1.0f || drawHeight < 1.0f) {
        ImGui::EndChild();
        return;
    }

    if (available.x > drawWidth) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - drawWidth) * 0.5f);
    }
    if (available.y > drawHeight) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (available.y - drawHeight) * 0.5f);
    }

    const ImVec2 uv0(worldState->cameraTileX / mapWidth, worldState->cameraTileY / mapHeight);
    const ImVec2 uv1((worldState->cameraTileX + visibleW) / mapWidth,
        (worldState->cameraTileY + visibleH) / mapHeight);

    ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture), ImVec2(drawWidth, drawHeight), uv0, uv1);

    const ImVec2 imageMin = ImGui::GetItemRectMin();
    const ImVec2 imageMax = ImGui::GetItemRectMax();
    const float imageW = imageMax.x - imageMin.x;
    const float imageH = imageMax.y - imageMin.y;

    worldState->viewportScreenX = imageMin.x;
    worldState->viewportScreenY = imageMin.y;
    worldState->viewportWidthPx = imageW;
    worldState->viewportHeightPx = imageH;
    worldState->visibleTileSpanX = visibleW;
    worldState->visibleTileSpanY = visibleH;
    worldState->viewportHovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (tool->GetChunkWidthTiles() > 0 && tool->GetChunkHeightTiles() > 0) {
        const ImU32 gridColorX = IM_COL32(255, 0, 255, 180);
        const ImU32 gridColorY = IM_COL32(30, 160, 255, 180);

        for (int tileX = 0; tileX <= worldState->map.width; tileX += tool->GetChunkWidthTiles()) {
            const float t = (static_cast<float>(tileX) - worldState->cameraTileX) / visibleW;
            if (t < 0.0f || t > 1.0f) {
                continue;
            }
            const float px = imageMin.x + t * imageW;
            drawList->AddLine(ImVec2(px, imageMin.y), ImVec2(px, imageMax.y), gridColorX);
        }
        for (int tileY = 0; tileY <= worldState->map.height; tileY += tool->GetChunkHeightTiles()) {
            const float t = (static_cast<float>(tileY) - worldState->cameraTileY) / visibleH;
            if (t < 0.0f || t > 1.0f) {
                continue;
            }
            const float py = imageMin.y + t * imageH;
            drawList->AddLine(ImVec2(imageMin.x, py), ImVec2(imageMax.x, py), gridColorY);
        }
    }

    if (worldState->viewportHovered && imageW > 0.0f && imageH > 0.0f) {
        const ImVec2 mousePos = ImGui::GetMousePos();
        const float relX = std::max(0.0f, std::min(1.0f, (mousePos.x - imageMin.x) / imageW));
        const float relY = std::max(0.0f, std::min(1.0f, (mousePos.y - imageMin.y) / imageH));

        int hoverX = static_cast<int>(std::floor(worldState->cameraTileX + relX * visibleW));
        int hoverY = static_cast<int>(std::floor(worldState->cameraTileY + relY * visibleH));
        hoverX = std::max(0, std::min(hoverX, worldState->map.width - 1));
        hoverY = std::max(0, std::min(hoverY, worldState->map.height - 1));

        worldState->hoverTileX = hoverX;
        worldState->hoverTileY = hoverY;
        worldState->hoverLayerName = FindTopLayerName(worldState->map, hoverX, hoverY);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            worldState->selectedTileX = hoverX;
            worldState->selectedTileY = hoverY;
        }
    }

    ImGui::EndChild();
}

}  // namespace cgul_demo
