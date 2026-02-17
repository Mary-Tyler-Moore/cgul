#pragma once

#include "chunkexporter/tools/ChunkExporterTool.hpp"
#include "world/WorldState.hpp"

#include <filesystem>
#include <string>

namespace cgul_demo {

bool SaveStateCgul(const std::filesystem::path& path, const WorldState& worldState,
    const tools::ChunkExporterTool& tool, std::string* outError);

bool LoadStateCgul(const std::filesystem::path& path, WorldState* worldState,
    tools::ChunkExporterTool* tool, std::string* outError);

}  // namespace cgul_demo
