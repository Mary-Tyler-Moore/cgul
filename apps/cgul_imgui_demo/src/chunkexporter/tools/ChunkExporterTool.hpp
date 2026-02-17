#pragma once

#include "chunkexporter/tiled/TiledMap.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace tools {

class ChunkExporterTool {
public:
    explicit ChunkExporterTool(
        SDL_Renderer* renderer,
        const std::filesystem::path& defaultBrowseDir = std::filesystem::path(),
        const std::filesystem::path& outputRoot = std::filesystem::path());
    ~ChunkExporterTool();

    // Standalone window mode.
    void Draw();
    // Embedded panel mode.
    void DrawContent(bool includePreview);

    bool EnsurePreviewReady();

    bool HasMap() const;
    const tiled::TiledMap& GetMap() const;
    const std::string& GetInputPath() const;
    const std::string& GetChunkType() const;

    const std::string& GetLoadError() const;
    const std::string& GetRenderError() const;
    const std::string& GetStatusText() const;
    const std::string& GetLastOutputPath() const;

    SDL_Texture* GetPreviewTexture() const;
    int GetPreviewTextureWidth() const;
    int GetPreviewTextureHeight() const;

    int GetChunkWidthTiles() const;
    int GetChunkHeightTiles() const;
    int GetTileSizeIndex() const;
    int GetSelectedTileSize() const;
    int GetSelectedTileSizePx() const;
    bool GetExportNonEmptyOnly() const;

    void SetInputPath(const std::string& path);
    bool LoadMapFromInputPath();
    void SetChunkType(const std::string& type);
    void SetChunkDimsTiles(int widthTiles, int heightTiles);
    void SetTileSizeIndex(int index);
    void SetExportNonEmptyOnly(bool value);

    const std::filesystem::path& GetOutputRoot() const;
    void SetOutputRoot(const std::filesystem::path& outputRoot);
    void SetBrowseDir(const std::filesystem::path& browseDir);

private:
    static constexpr int kPathBufferSize = 1024;
    static constexpr int kNameBufferSize = 128;

    std::array<char, kPathBufferSize> inputPath_{};
    std::array<char, kPathBufferSize> browseDir_{};
    std::vector<std::filesystem::path> browseFiles_;
    int browseSelection_ = -1;
    bool browseOpen_ = false;
    std::array<char, kNameBufferSize> filenamePrefix_{};

    tiled::TiledMap map_;
    bool hasMap_ = false;
    std::string loadError_;
    std::string statusText_;
    std::string lastOutputPath_;
    std::string renderError_;

    int chunkWidthTiles_ = 25;
    int chunkHeightTiles_ = 25;
    int tileSizeIndex_ = 1;  // 0=16, 1=32, 2=128
    bool exportNonEmptyOnly_ = true;
    float exportProgress_ = 0.0f;

    std::string chunkType_ = "island";
    std::filesystem::path outputRoot_;

    struct TilesetTexture {
        int firstGid = 0;
        int lastGid = 0;
        int tileCount = 0;
        int tileWidth = 0;
        int tileHeight = 0;
        int columns = 0;
        int atlasWidth = 0;
        int atlasHeight = 0;
        std::string imageName;
        SDL_Texture* texture = nullptr;
    };

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* previewTexture_ = nullptr;
    int previewTexWidth_ = 0;
    int previewTexHeight_ = 0;
    bool previewDirty_ = false;
    int hoverTileX_ = -1;
    int hoverTileY_ = -1;
    std::string hoverTopLayerName_;
    std::vector<TilesetTexture> tilesets_;
    mutable std::string inputPathViewCache_;

    void LoadMapFromPath();
    void ResetLoadedMap();
    void RefreshBrowseFiles();
    void DrawTilemapRender();
    void DrawTileInspector();
    bool ExportChunks();
    void ClearTilesets();
    bool LoadTilesets();
    const TilesetTexture* FindTilesetForGid(uint32_t gid) const;
    bool EnsurePreviewTexture();
    bool RenderTilemapPreview();
    std::string BuildFilenamePrefix() const;

    nlohmann::json BuildChunkJson(int tileX, int tileY, bool* nonEmpty) const;
    bool WriteJson(const std::filesystem::path& path, const nlohmann::json& doc,
        std::string* error) const;

    static std::string InferChunkType(const std::filesystem::path& path);
    static std::string ToLowerCopy(const std::string& value);
    static void SetPathBuffer(std::array<char, kPathBufferSize>* outBuffer,
        const std::filesystem::path& path);
};

}  // namespace tools
