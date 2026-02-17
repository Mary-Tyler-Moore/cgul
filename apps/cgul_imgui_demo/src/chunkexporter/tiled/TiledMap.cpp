#include "chunkexporter/tiled/TiledMap.hpp"

#include "chunkexporter/tiled/TiledLayerCodec.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace tiled {

bool LoadTiledMapFromJson(const std::filesystem::path& path, TiledMap* map, std::string* error) {
    if (!map) {
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "Failed to open map JSON: " + path.string();
        }
        return false;
    }

    nlohmann::json doc;
    try {
        file >> doc;
    } catch (const std::exception& ex) {
        if (error) {
            *error = std::string("Failed to parse map JSON: ") + ex.what();
        }
        return false;
    }

    if (!doc.contains("width") || !doc.contains("height") ||
        !doc.contains("tilewidth") || !doc.contains("tileheight")) {
        if (error) {
            *error = "Map JSON missing width/height/tilewidth/tileheight.";
        }
        return false;
    }

    map->width = doc["width"].get<int>();
    map->height = doc["height"].get<int>();
    map->tileWidth = doc["tilewidth"].get<int>();
    map->tileHeight = doc["tileheight"].get<int>();
    map->sourcePath = path;
    map->source = doc;

    map->tilesets.clear();
    if (doc.contains("tilesets") && doc["tilesets"].is_array()) {
        for (const auto& tileset : doc["tilesets"]) {
            map->tilesets.push_back(tileset);
        }
    }

    if (!doc.contains("layers") || !doc["layers"].is_array()) {
        if (error) {
            *error = "Map JSON missing layers array.";
        }
        return false;
    }

    map->layers.clear();
    for (const auto& layer : doc["layers"]) {
        TiledLayer layerInfo;
        layerInfo.name = layer.contains("name") && layer["name"].is_string()
            ? layer["name"].get<std::string>()
            : "layer";
        layerInfo.type = layer.contains("type") && layer["type"].is_string()
            ? layer["type"].get<std::string>()
            : "unknown";
        layerInfo.visible = layer.contains("visible") ? layer["visible"].get<bool>() : true;
        layerInfo.source = layer;
        layerInfo.isTileLayer = (layerInfo.type == "tilelayer");

        if (layerInfo.isTileLayer) {
            std::string decodeError;
            if (!DecodeLayerData(layer, map->width, map->height, &layerInfo.gids, &decodeError)) {
                if (error) {
                    *error = "Layer '" + layerInfo.name + "' decode failed: " + decodeError;
                }
                return false;
            }
            size_t nonZero = 0;
            for (uint32_t gid : layerInfo.gids) {
                if (gid != 0) {
                    ++nonZero;
                }
            }
            layerInfo.nonZeroCount = nonZero;
        }

        map->layers.push_back(std::move(layerInfo));
    }

    return true;
}

}  // namespace tiled
