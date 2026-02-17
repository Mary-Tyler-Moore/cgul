#pragma once

#include "chunkexporter/tools/ChunkExporterTool.hpp"

#include <filesystem>

struct SDL_Renderer;

namespace cgul_demo {

struct WorldState;

class ChunkExporterPanel {
public:
    explicit ChunkExporterPanel(
        SDL_Renderer* renderer,
        const std::filesystem::path& defaultBrowseDir,
        const std::filesystem::path& outputRoot);

    void Draw(WorldState* worldState);

    tools::ChunkExporterTool& Tool();
    const tools::ChunkExporterTool& Tool() const;

private:
    tools::ChunkExporterTool tool_;

    void SyncWorldState(WorldState* worldState) const;
};

}  // namespace cgul_demo
