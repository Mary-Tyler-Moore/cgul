#include "chunkexporter/tools/ChunkExporterTool.hpp"

#include "chunkexporter/tiled/AtlasTexture.hpp"

#include <SDL.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace tools {
namespace {

constexpr int kTileSizeOptions[] = {16, 32, 128};
constexpr const char* kTileSizeLabels[] = {"16", "32", "128"};
constexpr const char* kChunkTypeLabels[] = {"island", "water"};
constexpr size_t kChunkTypeCount = 2;
constexpr const char* kWaterLayers[] = {"ShallowWater", "DeepWater"};
constexpr const char* kIslandLayers[] = {"Huts", "Trees", "SandAndShore", "Sand", "Rocks"};
constexpr const char* kInspectorLayers[] = {
    "DeepWater", "ShallowWater", "Sand", "SandAndShore", "Rocks", "Trees", "Huts"};
constexpr size_t kInspectorLayerCount = sizeof(kInspectorLayers) / sizeof(kInspectorLayers[0]);
constexpr size_t kInspectorTreesIndex = 5;

bool LayerAllowedForChunkType(const std::string& chunkType, const std::string& layerName) {
    if (chunkType == "water") {
        for (const char* name : kWaterLayers) {
            if (layerName == name) {
                return true;
            }
        }
        return false;
    }
    if (chunkType == "island") {
        for (const char* name : kIslandLayers) {
            if (layerName == name) {
                return true;
            }
        }
        return false;
    }
    return true;
}

static inline uint32_t StripTiledFlags(uint32_t gid) {
    return gid & 0x1FFFFFFFu;
}

const tiled::TiledLayer* FindLayerByName(
    const std::vector<tiled::TiledLayer>& layers, const char* layerName) {
    for (std::vector<tiled::TiledLayer>::const_iterator it = layers.begin(); it != layers.end(); ++it) {
        if (it->name == layerName) {
            return &(*it);
        }
    }
    return nullptr;
}

uint32_t FindGidAt(const tiled::TiledLayer& layer, int mapWidth, int mapX, int mapY) {
    if (!layer.isTileLayer || !layer.visible || mapWidth <= 0 || mapX < 0 || mapY < 0) {
        return 0;
    }
    const size_t index = static_cast<size_t>(mapY * mapWidth + mapX);
    if (index >= layer.gids.size()) {
        return 0;
    }
    return StripTiledFlags(layer.gids[index]);
}

std::string FormatTimestamp() {
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    return oss.str();
}

bool ReadTextFile(const std::filesystem::path& path, std::string* outText, std::string* error) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        if (error) {
            *error = "Failed to open file: " + path.string();
        }
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    if (!file.good() && !file.eof()) {
        if (error) {
            *error = "Failed to read file: " + path.string();
        }
        return false;
    }
    if (outText) {
        *outText = ss.str();
    }
    return true;
}

bool ParseIntString(const std::string& value, int* outValue) {
    if (!outValue) {
        return false;
    }
    char* endPtr = nullptr;
    const long parsed = std::strtol(value.c_str(), &endPtr, 10);
    if (endPtr == value.c_str() || *endPtr != '\0') {
        return false;
    }
    *outValue = static_cast<int>(parsed);
    return true;
}

bool ExtractXmlAttribute(const std::string& xml, const char* tagName, const char* attributeName,
    std::string* outValue) {
    if (!tagName || !attributeName || !outValue) {
        return false;
    }

    const std::string tagPrefix = std::string("<") + tagName;
    const std::string attrPrefix = std::string(attributeName) + "=\"";
    size_t searchPos = 0;
    while (true) {
        const size_t tagPos = xml.find(tagPrefix, searchPos);
        if (tagPos == std::string::npos) {
            return false;
        }
        const size_t tagEnd = xml.find('>', tagPos);
        if (tagEnd == std::string::npos) {
            return false;
        }
        const size_t attrPos = xml.find(attrPrefix, tagPos);
        if (attrPos != std::string::npos && attrPos < tagEnd) {
            const size_t valueStart = attrPos + attrPrefix.size();
            const size_t valueEnd = xml.find('"', valueStart);
            if (valueEnd != std::string::npos && valueEnd <= tagEnd) {
                *outValue = xml.substr(valueStart, valueEnd - valueStart);
                return true;
            }
        }
        searchPos = tagEnd + 1;
    }
}

bool LoadExternalTilesetDef(const std::filesystem::path& mapPath, const nlohmann::json& mapTilesetEntry,
    nlohmann::json* outTilesetDef, std::filesystem::path* outSourcePath, std::string* outReason) {
    if (!outTilesetDef || !outSourcePath) {
        if (outReason) {
            *outReason = "Output pointers are null.";
        }
        return false;
    }

    const std::string sourceRel = mapTilesetEntry.value("source", "");
    if (sourceRel.empty()) {
        if (outReason) {
            *outReason = "tileset source is empty.";
        }
        return false;
    }

    const std::filesystem::path sourcePath = mapPath.parent_path() / sourceRel;
    std::string rawText;
    std::string fileError;
    if (!ReadTextFile(sourcePath, &rawText, &fileError)) {
        if (outReason) {
            *outReason = fileError;
        }
        return false;
    }

    const nlohmann::json parsedJson = nlohmann::json::parse(rawText, nullptr, false);
    if (!parsedJson.is_discarded() && parsedJson.is_object()) {
        *outTilesetDef = parsedJson;
        *outSourcePath = sourcePath;
        return true;
    }

    std::string tileWidthValue;
    std::string tileHeightValue;
    std::string columnsValue;
    std::string tileCountValue;
    std::string imageValue;
    if (!ExtractXmlAttribute(rawText, "tileset", "tilewidth", &tileWidthValue) ||
        !ExtractXmlAttribute(rawText, "tileset", "tileheight", &tileHeightValue) ||
        !ExtractXmlAttribute(rawText, "tileset", "tilecount", &tileCountValue) ||
        !ExtractXmlAttribute(rawText, "image", "source", &imageValue)) {
        if (outReason) {
            *outReason = "Unsupported TSX format or missing required attributes.";
        }
        return false;
    }

    int tileWidth = 0;
    int tileHeight = 0;
    int tileCount = 0;
    if (!ParseIntString(tileWidthValue, &tileWidth) || !ParseIntString(tileHeightValue, &tileHeight) ||
        !ParseIntString(tileCountValue, &tileCount)) {
        if (outReason) {
            *outReason = "Invalid numeric attribute(s) in TSX.";
        }
        return false;
    }

    int columns = 0;
    if (ExtractXmlAttribute(rawText, "tileset", "columns", &columnsValue)) {
        ParseIntString(columnsValue, &columns);
    }

    nlohmann::json tsxDef = nlohmann::json::object();
    tsxDef["tilewidth"] = tileWidth;
    tsxDef["tileheight"] = tileHeight;
    tsxDef["tilecount"] = tileCount;
    tsxDef["columns"] = columns;
    tsxDef["image"] = imageValue;

    *outTilesetDef = tsxDef;
    *outSourcePath = sourcePath;
    return true;
}

}  // namespace

ChunkExporterTool::ChunkExporterTool(
    SDL_Renderer* renderer,
    const std::filesystem::path& defaultBrowseDir,
    const std::filesystem::path& outputRoot)
    : renderer_(renderer) {
    std::error_code ec;
    std::filesystem::path resolvedBrowseDir = defaultBrowseDir;
    if (resolvedBrowseDir.empty()) {
        resolvedBrowseDir = std::filesystem::current_path(ec);
        if (ec || resolvedBrowseDir.empty()) {
            resolvedBrowseDir = std::filesystem::path(".");
        }
    }
    SetBrowseDir(resolvedBrowseDir);

    if (outputRoot.empty()) {
        outputRoot_ = resolvedBrowseDir / "chunks";
    } else {
        outputRoot_ = outputRoot;
    }

    ec.clear();
    const std::filesystem::path defaultMapPath = resolvedBrowseDir / "tilemap.json";
    if (std::filesystem::is_regular_file(defaultMapPath, ec)) {
        SetPathBuffer(&inputPath_, defaultMapPath);
    }
}

ChunkExporterTool::~ChunkExporterTool() {
    if (previewTexture_) {
        SDL_DestroyTexture(previewTexture_);
        previewTexture_ = nullptr;
    }
    ClearTilesets();
}

void ChunkExporterTool::Draw() {
    ImGui::Begin("Chunk Exporter");
    DrawContent(true);
    ImGui::End();
}

void ChunkExporterTool::DrawContent(bool includePreview) {
    ImGui::TextUnformatted("Input Map");
    ImGui::InputText("Map JSON", inputPath_.data(), inputPath_.size());
    if (ImGui::Button("Load")) {
        LoadMapFromPath();
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        browseOpen_ = true;
        RefreshBrowseFiles();
        ImGui::OpenPopup("Select JSON File");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ResetLoadedMap();
    }

    if (browseOpen_) {
        bool keepOpen = true;
        if (ImGui::BeginPopupModal("Select JSON File", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Directory", browseDir_.data(), browseDir_.size());
            ImGui::SameLine();
            if (ImGui::Button("Refresh")) {
                RefreshBrowseFiles();
            }

            ImGui::BeginChild("FileList", ImVec2(520.0f, 220.0f), true);
            if (browseFiles_.empty()) {
                ImGui::TextUnformatted("No .json files found in this directory.");
            } else {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(browseFiles_.size()));
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const std::string label = browseFiles_[static_cast<size_t>(i)].filename().string();
                        const bool selected = i == browseSelection_;
                        if (ImGui::Selectable(label.c_str(), selected)) {
                            browseSelection_ = i;
                        }
                    }
                }
            }
            ImGui::EndChild();

            if (ImGui::Button("Use Selected")) {
                if (browseSelection_ >= 0 && browseSelection_ < static_cast<int>(browseFiles_.size())) {
                    SetPathBuffer(&inputPath_, browseFiles_[static_cast<size_t>(browseSelection_)]);
                }
                browseOpen_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                browseOpen_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        browseOpen_ = keepOpen;
    }

    if (!loadError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", loadError_.c_str());
    }
    if (!renderError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s", renderError_.c_str());
    }

    if (hasMap_) {
        ImGui::Separator();
        ImGui::Text("Map Size: %d x %d tiles", map_.width, map_.height);
        ImGui::Text("Tile Size: %d x %d px", map_.tileWidth, map_.tileHeight);

        int chunkTypeIndex = chunkType_ == "water" ? 1 : 0;
        if (ImGui::Combo("Chunk Type", &chunkTypeIndex, kChunkTypeLabels, static_cast<int>(kChunkTypeCount))) {
            chunkType_ = kChunkTypeLabels[chunkTypeIndex];
            previewDirty_ = true;
        }

        if (includePreview) {
            ImGui::TextUnformatted("Tilemap Render");
            DrawTilemapRender();
            ImGui::Separator();
            ImGui::TextUnformatted("Tile Inspector");
            DrawTileInspector();
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Export Options");

    ImGui::InputInt("Chunk Width (tiles)", &chunkWidthTiles_);
    ImGui::InputInt("Chunk Height (tiles)", &chunkHeightTiles_);
    if (chunkWidthTiles_ < 1) {
        chunkWidthTiles_ = 1;
    }
    if (chunkHeightTiles_ < 1) {
        chunkHeightTiles_ = 1;
    }

    ImGui::Combo("Tile Size (px)", &tileSizeIndex_, kTileSizeLabels,
        static_cast<int>(sizeof(kTileSizeLabels) / sizeof(kTileSizeLabels[0])));

    if (hasMap_) {
        ImGui::InputText("Filename Prefix", filenamePrefix_.data(), filenamePrefix_.size());
    }

    const int selectedTileSize = kTileSizeOptions[tileSizeIndex_];
    if (hasMap_ && (map_.tileWidth != selectedTileSize || map_.tileHeight != selectedTileSize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Warning: map tile size is %dx%d", map_.tileWidth,
            map_.tileHeight);
    }

    ImGui::Checkbox("Export only non-empty chunks", &exportNonEmptyOnly_);
    ImGui::Text("Output Root: %s", outputRoot_.string().c_str());

    bool canExport = hasMap_;
    if (!canExport) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Export")) {
        ExportChunks();
    }
    if (!canExport) {
        ImGui::EndDisabled();
    }

    ImGui::ProgressBar(exportProgress_, ImVec2(-1.0f, 0.0f));
    if (!statusText_.empty()) {
        ImGui::TextWrapped("%s", statusText_.c_str());
    }
    if (!lastOutputPath_.empty()) {
        ImGui::TextWrapped("Last Output: %s", lastOutputPath_.c_str());
    }
}

bool ChunkExporterTool::EnsurePreviewReady() {
    if (!hasMap_) {
        return false;
    }
    if (previewDirty_) {
        return RenderTilemapPreview();
    }
    return previewTexture_ != nullptr;
}

bool ChunkExporterTool::HasMap() const {
    return hasMap_;
}

const tiled::TiledMap& ChunkExporterTool::GetMap() const {
    return map_;
}

const std::string& ChunkExporterTool::GetInputPath() const {
    inputPathViewCache_ = inputPath_.data();
    return inputPathViewCache_;
}

const std::string& ChunkExporterTool::GetChunkType() const {
    return chunkType_;
}

const std::string& ChunkExporterTool::GetLoadError() const {
    return loadError_;
}

const std::string& ChunkExporterTool::GetRenderError() const {
    return renderError_;
}

const std::string& ChunkExporterTool::GetStatusText() const {
    return statusText_;
}

const std::string& ChunkExporterTool::GetLastOutputPath() const {
    return lastOutputPath_;
}

SDL_Texture* ChunkExporterTool::GetPreviewTexture() const {
    return previewTexture_;
}

int ChunkExporterTool::GetPreviewTextureWidth() const {
    return previewTexWidth_;
}

int ChunkExporterTool::GetPreviewTextureHeight() const {
    return previewTexHeight_;
}

int ChunkExporterTool::GetChunkWidthTiles() const {
    return chunkWidthTiles_;
}

int ChunkExporterTool::GetChunkHeightTiles() const {
    return chunkHeightTiles_;
}

int ChunkExporterTool::GetTileSizeIndex() const {
    const int maxIndex = static_cast<int>(sizeof(kTileSizeOptions) / sizeof(kTileSizeOptions[0])) - 1;
    return std::max(0, std::min(tileSizeIndex_, maxIndex));
}

int ChunkExporterTool::GetSelectedTileSize() const {
    if (tileSizeIndex_ < 0 || tileSizeIndex_ >= static_cast<int>(sizeof(kTileSizeOptions) / sizeof(kTileSizeOptions[0]))) {
        return 32;
    }
    return kTileSizeOptions[tileSizeIndex_];
}

int ChunkExporterTool::GetSelectedTileSizePx() const {
    return GetSelectedTileSize();
}

bool ChunkExporterTool::GetExportNonEmptyOnly() const {
    return exportNonEmptyOnly_;
}

void ChunkExporterTool::SetInputPath(const std::string& path) {
    SetPathBuffer(&inputPath_, std::filesystem::path(path));
    inputPathViewCache_ = inputPath_.data();
}

bool ChunkExporterTool::LoadMapFromInputPath() {
    LoadMapFromPath();
    return hasMap_;
}

void ChunkExporterTool::SetChunkType(const std::string& type) {
    for (size_t i = 0; i < kChunkTypeCount; ++i) {
        if (type == kChunkTypeLabels[i]) {
            if (chunkType_ != type) {
                chunkType_ = type;
                previewDirty_ = true;
            }
            return;
        }
    }
}

void ChunkExporterTool::SetChunkDimsTiles(int widthTiles, int heightTiles) {
    const int clampedW = std::max(1, widthTiles);
    const int clampedH = std::max(1, heightTiles);
    if (chunkWidthTiles_ != clampedW || chunkHeightTiles_ != clampedH) {
        chunkWidthTiles_ = clampedW;
        chunkHeightTiles_ = clampedH;
        previewDirty_ = true;
    }
}

void ChunkExporterTool::SetTileSizeIndex(int index) {
    const int maxIndex = static_cast<int>(sizeof(kTileSizeOptions) / sizeof(kTileSizeOptions[0])) - 1;
    const int clamped = std::max(0, std::min(index, maxIndex));
    if (tileSizeIndex_ != clamped) {
        tileSizeIndex_ = clamped;
        previewDirty_ = true;
    }
}

void ChunkExporterTool::SetExportNonEmptyOnly(bool value) {
    exportNonEmptyOnly_ = value;
}

const std::filesystem::path& ChunkExporterTool::GetOutputRoot() const {
    return outputRoot_;
}

void ChunkExporterTool::SetOutputRoot(const std::filesystem::path& outputRoot) {
    if (outputRoot.empty()) {
        outputRoot_ = std::filesystem::path(browseDir_.data()) / "chunks";
        return;
    }
    outputRoot_ = outputRoot;
}

void ChunkExporterTool::SetBrowseDir(const std::filesystem::path& browseDir) {
    if (browseDir.empty()) {
        SetPathBuffer(&browseDir_, std::filesystem::path("."));
        return;
    }
    SetPathBuffer(&browseDir_, browseDir);
}

void ChunkExporterTool::LoadMapFromPath() {
    loadError_.clear();
    renderError_.clear();
    statusText_.clear();
    lastOutputPath_.clear();
    exportProgress_ = 0.0f;
    previewDirty_ = false;
    ClearTilesets();

    const std::filesystem::path inputPath(inputPath_.data());
    if (inputPath.empty()) {
        loadError_ = "Input path is empty.";
        hasMap_ = false;
        return;
    }

    tiled::TiledMap loaded;
    std::string error;
    if (!tiled::LoadTiledMapFromJson(inputPath, &loaded, &error)) {
        loadError_ = error;
        hasMap_ = false;
        return;
    }

    map_ = loaded;
    hasMap_ = true;
    statusText_ = "Loaded map: " + inputPath.string();
    chunkType_ = InferChunkType(inputPath);
    if (!LoadTilesets()) {
        if (renderError_.empty()) {
            renderError_ = "Failed to load tileset textures.";
        }
    } else {
        previewDirty_ = true;
    }

    const std::filesystem::path parentDir = inputPath.parent_path();
    if (!parentDir.empty()) {
        SetBrowseDir(parentDir);
    }
}

void ChunkExporterTool::ResetLoadedMap() {
    loadError_.clear();
    renderError_.clear();
    statusText_.clear();
    lastOutputPath_.clear();
    exportProgress_ = 0.0f;
    hasMap_ = false;
    map_ = tiled::TiledMap{};
    previewDirty_ = false;
    if (previewTexture_) {
        SDL_DestroyTexture(previewTexture_);
        previewTexture_ = nullptr;
    }
    previewTexWidth_ = 0;
    previewTexHeight_ = 0;
    hoverTileX_ = -1;
    hoverTileY_ = -1;
    hoverTopLayerName_.clear();
    ClearTilesets();
    inputPath_[0] = '\0';
    browseSelection_ = -1;
}

void ChunkExporterTool::RefreshBrowseFiles() {
    browseFiles_.clear();
    browseSelection_ = -1;

    const std::filesystem::path dirPath(browseDir_.data());
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".json") {
            browseFiles_.push_back(entry.path());
        }
    }

    std::sort(browseFiles_.begin(), browseFiles_.end());
}

void ChunkExporterTool::DrawTilemapRender() {
    hoverTileX_ = -1;
    hoverTileY_ = -1;
    hoverTopLayerName_.clear();

    ImGui::BeginChild("TilemapCanvas", ImVec2(0.0f, 320.0f), true);

    if (!hasMap_) {
        ImGui::TextUnformatted("Load a map to render tiles.");
        ImGui::EndChild();
        return;
    }

    if (!renderer_) {
        ImGui::TextUnformatted("Renderer not available.");
        ImGui::EndChild();
        return;
    }

    if (previewDirty_) {
        RenderTilemapPreview();
    }

    if (!previewTexture_) {
        ImGui::TextUnformatted("Tilemap preview unavailable.");
        ImGui::EndChild();
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float aspect = previewTexHeight_ > 0
        ? static_cast<float>(previewTexWidth_) / static_cast<float>(previewTexHeight_)
        : 1.0f;
    float drawWidth = available.x;
    float drawHeight = available.y;
    if (drawHeight > 0.0f && drawWidth > drawHeight * aspect) {
        drawWidth = drawHeight * aspect;
    } else if (drawWidth > 0.0f && drawHeight > drawWidth / aspect) {
        drawHeight = drawWidth / aspect;
    }

    ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture_), ImVec2(drawWidth, drawHeight));

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float imageW = max.x - min.x;
    const float imageH = max.y - min.y;

    if (imageW > 0.0f && imageH > 0.0f && map_.width > 0 && map_.height > 0) {
        if (ImGui::IsItemHovered()) {
            const ImVec2 mousePos = ImGui::GetMousePos();
            const float relX = (mousePos.x - min.x) / imageW;
            const float relY = (mousePos.y - min.y) / imageH;
            if (relX >= 0.0f && relX <= 1.0f && relY >= 0.0f && relY <= 1.0f) {
                int mapX = static_cast<int>(relX * static_cast<float>(map_.width));
                int mapY = static_cast<int>(relY * static_cast<float>(map_.height));
                mapX = std::max(0, std::min(mapX, map_.width - 1));
                mapY = std::max(0, std::min(mapY, map_.height - 1));
                hoverTileX_ = mapX;
                hoverTileY_ = mapY;

                for (std::vector<tiled::TiledLayer>::const_reverse_iterator it = map_.layers.rbegin();
                     it != map_.layers.rend(); ++it) {
                    if (!LayerAllowedForChunkType(chunkType_, it->name)) {
                        continue;
                    }
                    const uint32_t gid = FindGidAt(*it, map_.width, hoverTileX_, hoverTileY_);
                    if (gid != 0) {
                        hoverTopLayerName_ = it->name;
                        break;
                    }
                }
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 magenta = IM_COL32(255, 0, 255, 200);
        const ImU32 blue = IM_COL32(30, 160, 255, 200);

        if (chunkWidthTiles_ > 0) {
            for (int x = 0; x <= map_.width; x += chunkWidthTiles_) {
                const float px = min.x + imageW * (static_cast<float>(x) / static_cast<float>(map_.width));
                drawList->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), magenta);
            }
        }
        if (chunkHeightTiles_ > 0) {
            for (int y = 0; y <= map_.height; y += chunkHeightTiles_) {
                const float py = min.y + imageH * (static_cast<float>(y) / static_cast<float>(map_.height));
                drawList->AddLine(ImVec2(min.x, py), ImVec2(max.x, py), blue);
            }
        }

        if (hoverTileX_ >= 0 && hoverTileY_ >= 0) {
            const float x0 = min.x + imageW * (static_cast<float>(hoverTileX_) / static_cast<float>(map_.width));
            const float y0 = min.y + imageH * (static_cast<float>(hoverTileY_) / static_cast<float>(map_.height));
            const float x1 = min.x + imageW * (static_cast<float>(hoverTileX_ + 1) / static_cast<float>(map_.width));
            const float y1 = min.y + imageH * (static_cast<float>(hoverTileY_ + 1) / static_cast<float>(map_.height));
            drawList->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(255, 255, 0, 220), 0.0f, 0, 1.5f);
        }
    }

    ImGui::EndChild();
}

void ChunkExporterTool::DrawTileInspector() {
    ImGui::BeginChild("TileInspectorPanel", ImVec2(0.0f, 220.0f), true);

    if (!hasMap_) {
        ImGui::TextUnformatted("Load a map to inspect tiles.");
        ImGui::EndChild();
        return;
    }

    if (hoverTileX_ < 0 || hoverTileY_ < 0) {
        ImGui::TextUnformatted("Hover a tile in Tilemap Render to inspect layer gids.");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("Hover Tile: (%d, %d)", hoverTileX_, hoverTileY_);
    ImGui::Text("Top Layer: %s", hoverTopLayerName_.empty() ? "none" : hoverTopLayerName_.c_str());
    ImGui::Separator();

    uint32_t treesGid = 0;
    for (size_t i = 0; i < kInspectorLayerCount; ++i) {
        const char* layerName = kInspectorLayers[i];
        const tiled::TiledLayer* layer = FindLayerByName(map_.layers, layerName);
        const uint32_t gid = layer ? FindGidAt(*layer, map_.width, hoverTileX_, hoverTileY_) : 0;
        ImGui::Text("%-14s gid: %u", layerName, gid);
        if (i == kInspectorTreesIndex) {
            treesGid = gid;
        }
    }

    ImGui::Separator();
    if (treesGid == 0) {
        ImGui::TextUnformatted("Trees gid at hover: 0");
        ImGui::EndChild();
        return;
    }

    const TilesetTexture* tileset = FindTilesetForGid(treesGid);
    if (!tileset) {
        ImGui::Text("Trees gid %u did not match any tileset range.", treesGid);
        ImGui::EndChild();
        return;
    }

    const int localId = static_cast<int>(treesGid) - tileset->firstGid;
    if (localId < 0 || localId >= tileset->tileCount || tileset->columns <= 0) {
        ImGui::Text("Trees gid %u has invalid localId %d for tileset range [%d, %d].",
            treesGid, localId, tileset->firstGid, tileset->lastGid);
        ImGui::EndChild();
        return;
    }

    const int srcX = (localId % tileset->columns) * tileset->tileWidth;
    const int srcY = (localId / tileset->columns) * tileset->tileHeight;

    ImGui::Text("Trees: tileset=%s gid=%u localId=%d", tileset->imageName.c_str(), treesGid, localId);
    ImGui::Text("src rect: x=%d y=%d w=%d h=%d", srcX, srcY, tileset->tileWidth, tileset->tileHeight);

    if (!tileset->texture || tileset->atlasWidth <= 0 || tileset->atlasHeight <= 0) {
        ImGui::TextUnformatted("Trees tile preview unavailable (missing atlas dimensions/texture).");
        ImGui::EndChild();
        return;
    }

    const ImVec2 uv0(
        static_cast<float>(srcX) / static_cast<float>(tileset->atlasWidth),
        static_cast<float>(srcY) / static_cast<float>(tileset->atlasHeight));
    const ImVec2 uv1(
        static_cast<float>(srcX + tileset->tileWidth) / static_cast<float>(tileset->atlasWidth),
        static_cast<float>(srcY + tileset->tileHeight) / static_cast<float>(tileset->atlasHeight));
    const ImVec2 zoomSize(
        static_cast<float>(tileset->tileWidth * 4),
        static_cast<float>(tileset->tileHeight * 4));
    ImGui::Image(reinterpret_cast<ImTextureID>(tileset->texture), zoomSize, uv0, uv1);

    ImGui::EndChild();
}

void ChunkExporterTool::ClearTilesets() {
    for (TilesetTexture& tileset : tilesets_) {
        if (tileset.texture) {
            SDL_DestroyTexture(tileset.texture);
            tileset.texture = nullptr;
        }
    }
    tilesets_.clear();
}

bool ChunkExporterTool::LoadTilesets() {
    if (!renderer_) {
        renderError_ = "Renderer not available for tileset load.";
        return false;
    }
    if (map_.tilesets.empty()) {
        renderError_ = "Map has no tilesets.";
        return false;
    }

    tilesets_.clear();

    for (const nlohmann::json& tilesetJson : map_.tilesets) {
        if (!tilesetJson.contains("firstgid")) {
            continue;
        }
        const int firstGid = tilesetJson["firstgid"].get<int>();

        bool isExternal = false;
        nlohmann::json effectiveTilesetDef = tilesetJson;
        std::filesystem::path imageBasePath = map_.sourcePath.parent_path();
        if (tilesetJson.contains("source")) {
            isExternal = true;
            std::string externalReason;
            std::filesystem::path externalSourcePath;
            nlohmann::json externalDef;
            if (!LoadExternalTilesetDef(map_.sourcePath, tilesetJson, &externalDef, &externalSourcePath,
                    &externalReason)) {
#ifndef NDEBUG
                SDL_Log("Skipping external tileset '%s': %s",
                    tilesetJson.value("source", "").c_str(), externalReason.c_str());
#endif
                continue;
            }
            effectiveTilesetDef = externalDef;
            imageBasePath = externalSourcePath.parent_path();
        }

        const int tileWidth = effectiveTilesetDef.value("tilewidth", 0);
        const int tileHeight = effectiveTilesetDef.value("tileheight", 0);
        const int columns = effectiveTilesetDef.value("columns", 0);
        const std::string imageName = effectiveTilesetDef.value("image", "");
        const int tileCount = effectiveTilesetDef.value("tilecount", 0);

        if (tileWidth <= 0 || tileHeight <= 0 || imageName.empty()) {
#ifndef NDEBUG
            if (isExternal) {
                SDL_Log("Skipping external tileset '%s': missing tilewidth/tileheight/image.",
                    tilesetJson.value("source", "").c_str());
            }
#endif
            continue;
        }
        if (tileCount <= 0) {
            if (isExternal) {
#ifndef NDEBUG
                SDL_Log("Skipping external tileset '%s': missing a valid tilecount.",
                    tilesetJson.value("source", "").c_str());
#endif
                continue;
            } else {
                renderError_ = "Tileset '" + imageName + "' is missing a valid tilecount.";
                ClearTilesets();
                return false;
            }
        }

        const std::filesystem::path imagePath = imageBasePath / imageName;
        int atlasWidth = 0;
        int atlasHeight = 0;
        std::string error;
        SDL_Texture* texture =
            tiled::LoadAtlasTexture(renderer_, imagePath, &atlasWidth, &atlasHeight, &error);
        if (!texture) {
            if (isExternal) {
#ifndef NDEBUG
                SDL_Log("Skipping external tileset '%s': %s",
                    tilesetJson.value("source", "").c_str(),
                    (error.empty() ? "Failed to load atlas texture." : error.c_str()));
#endif
                continue;
            } else {
                renderError_ = error.empty() ? "Failed to load atlas texture." : error;
                ClearTilesets();
                return false;
            }
        }

        int resolvedColumns = columns;
        if (resolvedColumns <= 0 && atlasWidth > 0) {
            resolvedColumns = atlasWidth / tileWidth;
        }
        if (resolvedColumns <= 0) {
            SDL_DestroyTexture(texture);
#ifndef NDEBUG
            if (isExternal) {
                SDL_Log("Skipping external tileset '%s': invalid columns.",
                    tilesetJson.value("source", "").c_str());
            }
#endif
            continue;
        }

        TilesetTexture tileset;
        tileset.firstGid = firstGid;
        tileset.lastGid = firstGid + tileCount - 1;
        tileset.tileCount = tileCount;
        tileset.tileWidth = tileWidth;
        tileset.tileHeight = tileHeight;
        tileset.columns = resolvedColumns;
        tileset.atlasWidth = atlasWidth;
        tileset.atlasHeight = atlasHeight;
        tileset.imageName = imageName;
        tileset.texture = texture;
        tilesets_.push_back(tileset);
    }

    if (tilesets_.empty()) {
        renderError_ = "No usable tilesets found in map.";
        return false;
    }

    std::sort(tilesets_.begin(), tilesets_.end(), [](const TilesetTexture& a, const TilesetTexture& b) {
        return a.firstGid < b.firstGid;
    });

#ifndef NDEBUG
    SDL_Log("Tilesets loaded: %zu", tilesets_.size());
    for (size_t i = 0; i < tilesets_.size(); ++i) {
        const TilesetTexture& ts = tilesets_[i];
        SDL_Log("Tileset[%zu]: gid=[%d,%d] tileCount=%d columns=%d tile=%dx%d atlas=%dx%d image=%s texture=%p",
            i, ts.firstGid, ts.lastGid, ts.tileCount, ts.columns, ts.tileWidth, ts.tileHeight,
            ts.atlasWidth, ts.atlasHeight, ts.imageName.c_str(), static_cast<void*>(ts.texture));
    }
#endif

    return true;
}

const ChunkExporterTool::TilesetTexture* ChunkExporterTool::FindTilesetForGid(uint32_t gid) const {
    const TilesetTexture* best = nullptr;
    for (const TilesetTexture& tileset : tilesets_) {
        if (gid >= static_cast<uint32_t>(tileset.firstGid)) {
            if (!best || tileset.firstGid > best->firstGid) {
                best = &tileset;
            }
        }
    }
    return best;
}

bool ChunkExporterTool::EnsurePreviewTexture() {
    if (!renderer_) {
        return false;
    }
    if (!SDL_RenderTargetSupported(renderer_)) {
        renderError_ = "Renderer does not support render targets.";
        return false;
    }

    const int targetSize = 512;
    const int targetWidth = targetSize;
    int targetHeight = targetSize;
    if (map_.width > 0 && map_.height > 0) {
        targetHeight = static_cast<int>(targetSize * (static_cast<float>(map_.height) / static_cast<float>(map_.width)));
    }

    if (previewTexture_ && previewTexWidth_ == targetWidth && previewTexHeight_ == targetHeight) {
        return true;
    }

    if (previewTexture_) {
        SDL_DestroyTexture(previewTexture_);
        previewTexture_ = nullptr;
    }

    previewTexture_ = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, targetWidth, targetHeight);
    if (!previewTexture_) {
        renderError_ = std::string("Failed to create preview texture: ") + SDL_GetError();
        return false;
    }
    SDL_SetTextureBlendMode(previewTexture_, SDL_BLENDMODE_BLEND);
    previewTexWidth_ = targetWidth;
    previewTexHeight_ = targetHeight;
    return true;
}

bool ChunkExporterTool::RenderTilemapPreview() {
    if (!hasMap_ || tilesets_.empty()) {
        return false;
    }
    if (!EnsurePreviewTexture()) {
        return false;
    }

    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    if (SDL_SetRenderTarget(renderer_, previewTexture_) != 0) {
        renderError_ = std::string("Failed to set render target: ") + SDL_GetError();
        SDL_SetRenderTarget(renderer_, previousTarget);
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);

    if (map_.width <= 0 || map_.height <= 0) {
        SDL_SetRenderTarget(renderer_, previousTarget);
        return false;
    }

    const char* terrainOrder[] = {"DeepWater", "ShallowWater", "SandAndShore", "Sand", "Grass"};
    const char* overlayOrder[] = {"Rocks", "Trees", "Huts"};
    const size_t terrainCount = sizeof(terrainOrder) / sizeof(terrainOrder[0]);
    const size_t overlayCount = sizeof(overlayOrder) / sizeof(overlayOrder[0]);

    std::vector<const tiled::TiledLayer*> terrainLayers;
    terrainLayers.reserve(terrainCount);
    for (size_t i = 0; i < terrainCount; ++i) {
        terrainLayers.push_back(FindLayerByName(map_.layers, terrainOrder[i]));
    }

    std::vector<const tiled::TiledLayer*> overlayLayers;
    overlayLayers.reserve(overlayCount);
    for (size_t i = 0; i < overlayCount; ++i) {
        overlayLayers.push_back(FindLayerByName(map_.layers, overlayOrder[i]));
    }

#ifndef NDEBUG
    {
        const tiled::TiledLayer* treesLayer = FindLayerByName(map_.layers, "Trees");
        if (treesLayer && treesLayer->isTileLayer && treesLayer->visible) {
            size_t countNonZero = 0;
            size_t flaggedCount = 0;
            size_t countTilesetHit = 0;
            size_t treesSampleLogCount = 0;
            uint32_t minGid = 0;
            uint32_t maxGid = 0;
            bool haveRange = false;
            for (std::vector<uint32_t>::const_iterator it = treesLayer->gids.begin();
                 it != treesLayer->gids.end(); ++it) {
                const uint32_t rawGid = *it;
                const uint32_t gid = StripTiledFlags(rawGid);
                if (gid == 0) {
                    continue;
                }
                ++countNonZero;
                if (rawGid != StripTiledFlags(rawGid)) {
                    ++flaggedCount;
                }
                if (!haveRange) {
                    minGid = gid;
                    maxGid = gid;
                    haveRange = true;
                } else {
                    minGid = std::min(minGid, gid);
                    maxGid = std::max(maxGid, gid);
                }
                const TilesetTexture* selected = FindTilesetForGid(gid);
                if (selected) {
                    const int localId = static_cast<int>(gid) - selected->firstGid;
                    if (localId >= 0 && localId < selected->tileCount) {
                        ++countTilesetHit;
                        if (treesSampleLogCount < 6) {
                            ++treesSampleLogCount;
                            SDL_Log("Trees gid sample: gid=%u selectedFirstGid=%d localId=%d",
                                gid, selected->firstGid, localId);
                        }
                    }
                }
            }
            SDL_Log("Trees gids nonzero: %zu, gid range: [%u,%u], tileset hits: %zu, flags set: %zu",
                countNonZero, haveRange ? minGid : 0u, haveRange ? maxGid : 0u,
                countTilesetHit, flaggedCount);
        } else {
            SDL_Log("Trees layer missing, not a visible tile layer, or empty.");
        }

    }
#endif

    for (int py = 0; py < previewTexHeight_; ++py) {
        const int mapY = py * map_.height / previewTexHeight_;
        for (int px = 0; px < previewTexWidth_; ++px) {
            const int mapX = px * map_.width / previewTexWidth_;
            SDL_FRect dst = {static_cast<float>(px), static_cast<float>(py), 1.0f, 1.0f};
            bool drewAny = false;

            const auto drawGid = [&](uint32_t gid) {
                if (gid == 0) {
                    return;
                }

                const TilesetTexture* tileset = FindTilesetForGid(gid);
                if (!tileset || tileset->columns <= 0) {
                    return;
                }

                const int localId = static_cast<int>(gid) - tileset->firstGid;
                if (localId < 0 || localId >= tileset->tileCount) {
                    return;
                }

                const int srcX = (localId % tileset->columns) * tileset->tileWidth;
                const int srcY = (localId / tileset->columns) * tileset->tileHeight;
                const int cx = srcX + tileset->tileWidth / 2;
                const int cy = srcY + tileset->tileHeight / 2;
                SDL_Rect src = {cx, cy, 1, 1};
                SDL_RenderCopyF(renderer_, tileset->texture, &src, &dst);
                drewAny = true;
            };

            for (std::vector<const tiled::TiledLayer*>::const_iterator it = terrainLayers.begin();
                 it != terrainLayers.end(); ++it) {
                const tiled::TiledLayer* layer = *it;
                if (!layer || !LayerAllowedForChunkType(chunkType_, layer->name)) {
                    continue;
                }
                drawGid(FindGidAt(*layer, map_.width, mapX, mapY));
            }

            for (std::vector<const tiled::TiledLayer*>::const_iterator it = overlayLayers.begin();
                 it != overlayLayers.end(); ++it) {
                const tiled::TiledLayer* layer = *it;
                if (!layer || !LayerAllowedForChunkType(chunkType_, layer->name)) {
                    continue;
                }
                drawGid(FindGidAt(*layer, map_.width, mapX, mapY));
            }

            if (!drewAny) {
                for (std::vector<tiled::TiledLayer>::const_reverse_iterator layerIt = map_.layers.rbegin();
                     layerIt != map_.layers.rend(); ++layerIt) {
                    if (!LayerAllowedForChunkType(chunkType_, layerIt->name)) {
                        continue;
                    }
                    const uint32_t gid = FindGidAt(*layerIt, map_.width, mapX, mapY);
                    if (gid != 0) {
                        drawGid(gid);
                        break;
                    }
                }
            }
        }
    }

    SDL_SetRenderTarget(renderer_, previousTarget);
    previewDirty_ = false;
    return true;
}

bool ChunkExporterTool::ExportChunks() {
    if (!hasMap_) {
        return false;
    }

    const int tileSizePx = GetSelectedTileSize();
    const int mapCenterX = map_.width / 2;
    const int mapCenterY = map_.height / 2;

    const std::string folderName = std::string("gen_") + FormatTimestamp();
    const std::filesystem::path outputDir = outputRoot_ / folderName;
    const std::filesystem::path allDir = outputDir / "chunks";
    const std::filesystem::path nonEmptyDir = outputDir / "chunks_non_empty";
    const std::filesystem::path outDir = exportNonEmptyOnly_ ? nonEmptyDir : allDir;

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    if (ec) {
        statusText_ = "Failed to create output directories: " + ec.message();
        return false;
    }

    const int chunksX = (map_.width + chunkWidthTiles_ - 1) / chunkWidthTiles_;
    const int chunksY = (map_.height + chunkHeightTiles_ - 1) / chunkHeightTiles_;
    const int totalChunks = chunksX * chunksY;

    int nonEmptyCount = 0;
    int writtenCount = 0;
    int chunkIndex = 0;

    exportProgress_ = 0.0f;

    const std::string prefix = BuildFilenamePrefix();
    for (int tileY = 0; tileY < map_.height; tileY += chunkHeightTiles_) {
        for (int tileX = 0; tileX < map_.width; tileX += chunkWidthTiles_) {
            bool nonEmpty = false;
            nlohmann::json chunk = BuildChunkJson(tileX, tileY, &nonEmpty);

            const int worldPxX = (tileX - mapCenterX) * tileSizePx;
            const int worldPxY = (tileY - mapCenterY) * tileSizePx;
            const std::string baseName = prefix.empty() ? chunkType_ : prefix;
            const std::string filename = baseName + "_chunk_" + std::to_string(tileX) + "_" +
                std::to_string(tileY) + "_" + std::to_string(worldPxX) + "_" +
                std::to_string(worldPxY) + ".json";

            if (exportNonEmptyOnly_ && !nonEmpty) {
                ++chunkIndex;
                exportProgress_ = totalChunks > 0
                    ? static_cast<float>(chunkIndex) / static_cast<float>(totalChunks)
                    : 1.0f;
                continue;
            }

            std::string writeError;
            if (!WriteJson(outDir / filename, chunk, &writeError)) {
                statusText_ = "Write failed: " + writeError;
                return false;
            }

            if (nonEmpty) {
                ++nonEmptyCount;
            }
            ++writtenCount;
            ++chunkIndex;
            exportProgress_ = totalChunks > 0
                ? static_cast<float>(chunkIndex) / static_cast<float>(totalChunks)
                : 1.0f;
        }
    }

    std::ostringstream status;
    if (exportNonEmptyOnly_) {
        status << "Export complete. Wrote " << writtenCount
               << " non-empty chunks to " << nonEmptyDir.string();
    } else {
        status << "Export complete. Wrote " << writtenCount
               << " chunks to " << allDir.string() << " (non-empty: " << nonEmptyCount << ")";
    }
    statusText_ = status.str();
    lastOutputPath_ = outDir.string();
    return true;
}

nlohmann::json ChunkExporterTool::BuildChunkJson(int tileX, int tileY, bool* nonEmpty) const {
    nlohmann::json chunk = map_.source;
    chunk["width"] = chunkWidthTiles_;
    chunk["height"] = chunkHeightTiles_;
    chunk["tilewidth"] = map_.tileWidth;
    chunk["tileheight"] = map_.tileHeight;
    chunk["offsetX"] = tileX;
    chunk["offsetY"] = tileY;
    chunk["tilesets"] = map_.tilesets;

    bool anyNonZero = false;
    nlohmann::json layers = nlohmann::json::array();

    for (const tiled::TiledLayer& layer : map_.layers) {
        if (!layer.isTileLayer) {
            continue;
        }
        nlohmann::json outLayer = layer.source;
        std::vector<uint32_t> chunkData;
        chunkData.reserve(static_cast<size_t>(chunkWidthTiles_ * chunkHeightTiles_));

        for (int y = 0; y < chunkHeightTiles_; ++y) {
            const int mapY = tileY + y;
            for (int x = 0; x < chunkWidthTiles_; ++x) {
                const int mapX = tileX + x;
                uint32_t gid = 0;
                if (mapX >= 0 && mapX < map_.width && mapY >= 0 && mapY < map_.height) {
                    const size_t index = static_cast<size_t>(mapY * map_.width + mapX);
                    if (index < layer.gids.size()) {
                        gid = StripTiledFlags(layer.gids[index]);
                    }
                }
                if (gid != 0) {
                    anyNonZero = true;
                }
                chunkData.push_back(gid);
            }
        }

        outLayer["width"] = chunkWidthTiles_;
        outLayer["height"] = chunkHeightTiles_;
        outLayer["data"] = chunkData;
        outLayer.erase("encoding");
        outLayer.erase("compression");
        layers.push_back(outLayer);
    }

    chunk["layers"] = layers;

    if (nonEmpty) {
        *nonEmpty = anyNonZero;
    }

    return chunk;
}

bool ChunkExporterTool::WriteJson(
    const std::filesystem::path& path, const nlohmann::json& doc, std::string* error) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "Failed to open output file: " + path.string();
        }
        return false;
    }

    file << doc.dump(2);
    return true;
}

std::string ChunkExporterTool::InferChunkType(const std::filesystem::path& path) {
    const std::string lower = ToLowerCopy(path.filename().string());
    if (lower.find("water") != std::string::npos) {
        return "water";
    }
    if (lower.find("island") != std::string::npos) {
        return "island";
    }
    return "island";
}

std::string ChunkExporterTool::ToLowerCopy(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string ChunkExporterTool::BuildFilenamePrefix() const {
    const std::string raw(filenamePrefix_.data());
    std::string sanitized;
    sanitized.reserve(raw.size());
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            sanitized.push_back(static_cast<char>(c));
        }
    }
    while (!sanitized.empty() && (sanitized.back() == '_' || sanitized.back() == '-')) {
        sanitized.pop_back();
    }
    return sanitized;
}

void ChunkExporterTool::SetPathBuffer(
    std::array<char, kPathBufferSize>* outBuffer, const std::filesystem::path& path) {
    if (!outBuffer) {
        return;
    }
    const std::string pathStr = path.string();
    std::snprintf(outBuffer->data(), outBuffer->size(), "%s", pathStr.c_str());
}

}  // namespace tools
