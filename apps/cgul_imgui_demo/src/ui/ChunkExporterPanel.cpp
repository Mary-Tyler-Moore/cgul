#include "ui/ChunkExporterPanel.hpp"

#include "world/WorldState.hpp"

#include <imgui.h>

namespace cgul_demo {

ChunkExporterPanel::ChunkExporterPanel(
    SDL_Renderer* renderer,
    const std::filesystem::path& defaultBrowseDir,
    const std::filesystem::path& outputRoot)
    : tool_(renderer, defaultBrowseDir, outputRoot) {
}

void ChunkExporterPanel::Draw(WorldState* worldState) {
    ImGui::Begin("Chunk Exporter");
    tool_.DrawContent(false);
    ImGui::End();

    SyncWorldState(worldState);
}

tools::ChunkExporterTool& ChunkExporterPanel::Tool() {
    return tool_;
}

const tools::ChunkExporterTool& ChunkExporterPanel::Tool() const {
    return tool_;
}

void ChunkExporterPanel::SyncWorldState(WorldState* worldState) const {
    if (!worldState) {
        return;
    }

    worldState->hasMap = tool_.HasMap();
    if (worldState->hasMap) {
        worldState->map = tool_.GetMap();
    } else {
        worldState->map = tiled::TiledMap{};
        worldState->cameraTileX = 0.0f;
        worldState->cameraTileY = 0.0f;
        worldState->zoom = 1.0f;
        worldState->ResetHover();
    }

    worldState->statusText = tool_.GetStatusText();
    if (!tool_.GetLoadError().empty()) {
        worldState->errorText = tool_.GetLoadError();
    } else if (!tool_.GetRenderError().empty()) {
        worldState->errorText = tool_.GetRenderError();
    } else {
        worldState->errorText.clear();
    }

    worldState->ClampCameraToMap();
}

}  // namespace cgul_demo
