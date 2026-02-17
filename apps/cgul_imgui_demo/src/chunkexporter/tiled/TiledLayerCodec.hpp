#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tiled {

bool DecodeLayerData(const nlohmann::json& layer, int width, int height,
    std::vector<uint32_t>* outGids, std::string* error);

}  // namespace tiled
