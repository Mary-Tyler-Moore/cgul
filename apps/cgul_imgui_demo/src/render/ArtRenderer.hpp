#pragma once

namespace tools {
class ChunkExporterTool;
}

namespace cgul_demo {

struct WorldState;

class ArtRenderer {
public:
    void Draw(WorldState* worldState, tools::ChunkExporterTool* tool);
};

}  // namespace cgul_demo
