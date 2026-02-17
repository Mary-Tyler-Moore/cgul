#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tiled {

struct TiledLayer {
    std::string name;
    std::string type;
    bool visible = true;
    bool isTileLayer = false;
    size_t nonZeroCount = 0;
    std::vector<uint32_t> gids;
    nlohmann::json source;
};

struct TiledMap {
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    std::filesystem::path sourcePath;
    nlohmann::json source;
    std::vector<nlohmann::json> tilesets;
    std::vector<TiledLayer> layers;
};

bool LoadTiledMapFromJson(const std::filesystem::path& path, TiledMap* map, std::string* error);

}  // namespace tiled
