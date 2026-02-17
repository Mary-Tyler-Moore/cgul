#include "chunkexporter/tiled/TiledLayerCodec.hpp"

#include <zlib.h>

#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace tiled {
namespace {

int Base64Value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

std::vector<uint8_t> DecodeBase64(const std::string& input) {
    std::vector<uint8_t> output;
    int value = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            continue;
        }
        if (c == '=') {
            break;
        }
        const int decoded = Base64Value(c);
        if (decoded < 0) {
            continue;
        }
        value = (value << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<uint8_t>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return output;
}

bool DecompressZlib(const std::vector<uint8_t>& input, size_t expectedSize,
    std::vector<uint8_t>* output) {
    output->assign(expectedSize, 0);
    uLongf destLen = static_cast<uLongf>(expectedSize);
    const int result = uncompress(output->data(), &destLen, input.data(),
        static_cast<uLongf>(input.size()));
    if (result != Z_OK) {
        return false;
    }
    output->resize(static_cast<size_t>(destLen));
    return true;
}

}  // namespace

bool DecodeLayerData(const nlohmann::json& layer, int width, int height,
    std::vector<uint32_t>* outGids, std::string* error) {
    if (!outGids) {
        return false;
    }

    const size_t expectedTiles = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t expectedBytes = expectedTiles * 4;
    outGids->clear();
    outGids->reserve(expectedTiles);

    if (!layer.contains("data")) {
        if (error) {
            *error = "Layer missing data field.";
        }
        return false;
    }

    if (layer["data"].is_array()) {
        const auto& dataArray = layer["data"];
        if (dataArray.size() != expectedTiles) {
            if (error) {
                *error = "Layer has unexpected tile count.";
            }
            return false;
        }
        for (const auto& entry : dataArray) {
            if (!entry.is_number()) {
                if (error) {
                    *error = "Layer has non-numeric tile data.";
                }
                return false;
            }
            outGids->push_back(entry.get<uint32_t>());
        }
        return true;
    }

    if (!layer["data"].is_string()) {
        if (error) {
            *error = "Layer data is neither array nor string.";
        }
        return false;
    }

    const std::string encoding =
        layer.contains("encoding") && layer["encoding"].is_string()
        ? layer["encoding"].get<std::string>()
        : "";
    const std::optional<std::string> compression =
        layer.contains("compression") && layer["compression"].is_string()
        ? std::optional<std::string>(layer["compression"].get<std::string>())
        : std::nullopt;

    if (encoding != "base64") {
        if (error) {
            *error = "Unsupported layer encoding: " + encoding;
        }
        return false;
    }

    const std::string dataStr = layer["data"].get<std::string>();
    const std::vector<uint8_t> decoded = DecodeBase64(dataStr);
    std::vector<uint8_t> raw;

    if (compression && *compression == "zlib") {
        if (!DecompressZlib(decoded, expectedBytes, &raw)) {
            if (error) {
                *error = "Layer zlib decompress failed.";
            }
            return false;
        }
    } else if (!compression || compression->empty()) {
        raw = decoded;
    } else {
        if (error) {
            *error = "Unsupported layer compression: " + *compression;
        }
        return false;
    }

    if (raw.size() != expectedBytes) {
        if (error) {
            *error = "Layer data size mismatch.";
        }
        return false;
    }

    for (size_t i = 0; i + 3 < raw.size(); i += 4) {
        const uint32_t gid = static_cast<uint32_t>(raw[i]) |
            (static_cast<uint32_t>(raw[i + 1]) << 8) |
            (static_cast<uint32_t>(raw[i + 2]) << 16) |
            (static_cast<uint32_t>(raw[i + 3]) << 24);
        outGids->push_back(gid);
    }

    return true;
}

}  // namespace tiled
