#include "app/DemoPersistence.hpp"

#include "cgul/core/equality.h"
#include "cgul/io/cgul_document.h"
#include "cgul/validate/validate.h"

#include <SDL.h>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

namespace cgul_demo {
namespace {

constexpr const char* kMetaCameraTileX = "cgul_imgui_demo.cameraTileX";
constexpr const char* kMetaCameraTileY = "cgul_imgui_demo.cameraTileY";
constexpr const char* kMetaZoom = "cgul_imgui_demo.zoom";
constexpr const char* kMetaInputPath = "cgul_imgui_demo.inputPath";
constexpr const char* kMetaChunkType = "cgul_imgui_demo.chunkType";
constexpr const char* kMetaChunkWidthTiles = "cgul_imgui_demo.chunkWidthTiles";
constexpr const char* kMetaChunkHeightTiles = "cgul_imgui_demo.chunkHeightTiles";
constexpr const char* kMetaTileSizeIndex = "cgul_imgui_demo.tileSizeIndex";
constexpr const char* kMetaExportNonEmptyOnly = "cgul_imgui_demo.exportNonEmptyOnly";
constexpr const char* kMetaOutputRoot = "cgul_imgui_demo.outputRoot";

bool SetError(const std::string& message, std::string* outError) {
    if (outError) {
        *outError = message;
    }
    return false;
}

std::string FloatToString(float value) {
    std::ostringstream os;
    os.setf(std::ios::fixed, std::ios::floatfield);
    os.precision(6);
    os << value;
    return os.str();
}

bool ParseIntString(const std::string& text, int* outValue) {
    if (!outValue || text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    *outValue = static_cast<int>(parsed);
    return true;
}

bool ParseFloatString(const std::string& text, float* outValue) {
    if (!outValue || text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    *outValue = parsed;
    return true;
}

bool ParseBoolString(const std::string& text, bool* outValue) {
    if (!outValue) {
        return false;
    }
    if (text == "1" || text == "true" || text == "TRUE") {
        *outValue = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "FALSE") {
        *outValue = false;
        return true;
    }
    return false;
}

bool ReadMetaString(const cgul::CgulDocument& doc, const std::string& key, std::string* outValue,
    std::string* outError) {
    if (!outValue) {
        return SetError("ReadMetaString: outValue is null", outError);
    }
    const auto it = doc.meta.find(key);
    if (it == doc.meta.end()) {
        return SetError("Missing metadata key: " + key, outError);
    }
    *outValue = it->second;
    return true;
}

bool ReadMetaInt(const cgul::CgulDocument& doc, const std::string& key, int* outValue, std::string* outError) {
    std::string value;
    if (!ReadMetaString(doc, key, &value, outError)) {
        return false;
    }
    if (!ParseIntString(value, outValue)) {
        return SetError("Invalid integer metadata for key: " + key, outError);
    }
    return true;
}

bool ReadMetaFloat(
    const cgul::CgulDocument& doc, const std::string& key, float* outValue, std::string* outError) {
    std::string value;
    if (!ReadMetaString(doc, key, &value, outError)) {
        return false;
    }
    if (!ParseFloatString(value, outValue)) {
        return SetError("Invalid float metadata for key: " + key, outError);
    }
    return true;
}

bool ReadMetaBool(const cgul::CgulDocument& doc, const std::string& key, bool* outValue, std::string* outError) {
    std::string value;
    if (!ReadMetaString(doc, key, &value, outError)) {
        return false;
    }
    if (!ParseBoolString(value, outValue)) {
        return SetError("Invalid boolean metadata for key: " + key, outError);
    }
    return true;
}

cgul::CgulDocument BuildStateDocument(const WorldState& worldState, const tools::ChunkExporterTool& tool) {
    cgul::CgulDocument doc;
    doc.gridWCells = 1;
    doc.gridHCells = 1;
    doc.seed = 0;
    doc.meta[kMetaCameraTileX] = FloatToString(worldState.cameraTileX);
    doc.meta[kMetaCameraTileY] = FloatToString(worldState.cameraTileY);
    doc.meta[kMetaZoom] = FloatToString(worldState.zoom);
    doc.meta[kMetaInputPath] = tool.GetInputPath();
    doc.meta[kMetaChunkType] = tool.GetChunkType();
    doc.meta[kMetaChunkWidthTiles] = std::to_string(tool.GetChunkWidthTiles());
    doc.meta[kMetaChunkHeightTiles] = std::to_string(tool.GetChunkHeightTiles());
    doc.meta[kMetaTileSizeIndex] = std::to_string(tool.GetTileSizeIndex());
    doc.meta[kMetaExportNonEmptyOnly] = tool.GetExportNonEmptyOnly() ? "true" : "false";
    if (!tool.GetOutputRoot().empty()) {
        doc.meta[kMetaOutputRoot] = tool.GetOutputRoot().string();
    }
    return doc;
}

}  // namespace

bool SaveStateCgul(const std::filesystem::path& path, const WorldState& worldState,
    const tools::ChunkExporterTool& tool, std::string* outError) {
    if (outError) {
        outError->clear();
    }

    cgul::CgulDocument doc = BuildStateDocument(worldState, tool);

    std::string error;
    if (!cgul::Validate(doc, &error)) {
        SDL_Log("State save validation failed: %s", error.c_str());
        return SetError(error, outError);
    }

    std::error_code ec;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            const std::string message = "Failed creating state directory '" + parent.string() + "': " +
                ec.message();
            SDL_Log("%s", message.c_str());
            return SetError(message, outError);
        }
    }

    if (!cgul::SaveCgulFile(path.string(), doc, &error)) {
        SDL_Log("State save failed: %s", error.c_str());
        return SetError(error, outError);
    }

    cgul::CgulDocument reloaded;
    if (!cgul::LoadCgulFile(path.string(), &reloaded, &error)) {
        SDL_Log("State round-trip load failed: %s", error.c_str());
        return SetError(error, outError);
    }

    if (!cgul::Validate(reloaded, &error)) {
        SDL_Log("State round-trip validate failed: %s", error.c_str());
        return SetError(error, outError);
    }

    std::string diff;
    if (!cgul::Equal(doc, reloaded, &diff)) {
        SDL_Log("State round-trip FAIL: %s", diff.c_str());
        return SetError(diff, outError);
    }

    SDL_Log(
        "Round-trip PASS: %s | camera=(%.2f,%.2f) zoom=%.2f chunk=%dx%d tileIdx=%d chunkType=%s input=%s",
        path.string().c_str(), worldState.cameraTileX, worldState.cameraTileY, worldState.zoom,
        tool.GetChunkWidthTiles(), tool.GetChunkHeightTiles(), tool.GetTileSizeIndex(),
        tool.GetChunkType().c_str(), tool.GetInputPath().c_str());
    return true;
}

bool LoadStateCgul(const std::filesystem::path& path, WorldState* worldState, tools::ChunkExporterTool* tool,
    std::string* outError) {
    if (outError) {
        outError->clear();
    }
    if (!worldState || !tool) {
        return SetError("LoadStateCgul: null world/tool", outError);
    }

    std::string error;
    cgul::CgulDocument doc;
    if (!cgul::LoadCgulFile(path.string(), &doc, &error)) {
        SDL_Log("State load failed: %s", error.c_str());
        return SetError(error, outError);
    }
    if (!cgul::Validate(doc, &error)) {
        SDL_Log("State load validation failed: %s", error.c_str());
        return SetError(error, outError);
    }

    std::string inputPath;
    std::string chunkType;
    int chunkW = tool->GetChunkWidthTiles();
    int chunkH = tool->GetChunkHeightTiles();
    int tileSizeIndex = tool->GetTileSizeIndex();
    bool exportNonEmpty = tool->GetExportNonEmptyOnly();
    float cameraTileX = worldState->cameraTileX;
    float cameraTileY = worldState->cameraTileY;
    float zoom = worldState->zoom;
    std::string outputRoot;

    if (!ReadMetaFloat(doc, kMetaCameraTileX, &cameraTileX, outError) ||
        !ReadMetaFloat(doc, kMetaCameraTileY, &cameraTileY, outError) ||
        !ReadMetaFloat(doc, kMetaZoom, &zoom, outError) ||
        !ReadMetaString(doc, kMetaInputPath, &inputPath, outError) ||
        !ReadMetaString(doc, kMetaChunkType, &chunkType, outError) ||
        !ReadMetaInt(doc, kMetaChunkWidthTiles, &chunkW, outError) ||
        !ReadMetaInt(doc, kMetaChunkHeightTiles, &chunkH, outError) ||
        !ReadMetaInt(doc, kMetaTileSizeIndex, &tileSizeIndex, outError) ||
        !ReadMetaBool(doc, kMetaExportNonEmptyOnly, &exportNonEmpty, outError)) {
        SDL_Log("State load metadata parse failed: %s", outError ? outError->c_str() : "unknown");
        return false;
    }

    const auto outputIt = doc.meta.find(kMetaOutputRoot);
    if (outputIt != doc.meta.end()) {
        outputRoot = outputIt->second;
    }

    tool->SetInputPath(inputPath);
    tool->LoadMapFromInputPath();
    tool->SetChunkType(chunkType);
    tool->SetChunkDimsTiles(chunkW, chunkH);
    tool->SetTileSizeIndex(tileSizeIndex);
    tool->SetExportNonEmptyOnly(exportNonEmpty);
    if (!outputRoot.empty()) {
        tool->SetOutputRoot(std::filesystem::path(outputRoot));
    }

    worldState->hasMap = tool->HasMap();
    if (worldState->hasMap) {
        worldState->map = tool->GetMap();
    } else {
        worldState->map = tiled::TiledMap{};
    }
    worldState->cameraTileX = cameraTileX;
    worldState->cameraTileY = cameraTileY;
    worldState->zoom = zoom;
    worldState->ClampCameraToMap();

    SDL_Log(
        "Loaded state: %s | camera=(%.2f,%.2f) zoom=%.2f chunk=%dx%d tileIdx=%d chunkType=%s input=%s hasMap=%d",
        path.string().c_str(), worldState->cameraTileX, worldState->cameraTileY, worldState->zoom,
        tool->GetChunkWidthTiles(), tool->GetChunkHeightTiles(), tool->GetTileSizeIndex(),
        tool->GetChunkType().c_str(), tool->GetInputPath().c_str(), tool->HasMap() ? 1 : 0);
    return true;
}

}  // namespace cgul_demo
