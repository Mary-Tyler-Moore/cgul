#pragma once

namespace tools {
class ChunkExporterTool;
}

namespace cgul_demo {

struct WorldState;

class CgulUiRenderer {
public:
    void Draw(WorldState* worldState, const tools::ChunkExporterTool* tool);
};

}  // namespace cgul_demo
