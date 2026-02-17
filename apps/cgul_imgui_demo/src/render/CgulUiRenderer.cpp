#include "render/CgulUiRenderer.hpp"

#include "chunkexporter/tools/ChunkExporterTool.hpp"
#include "world/WorldState.hpp"

#include "cgul/core/frame.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace cgul_demo {
namespace {

struct TileVisual {
    char glyph;
    const char* layerName;
    cgul::Rgba8 fg;
    cgul::Rgba8 bg;
};

struct TileLookup {
    bool hasTile;
    TileVisual visual;
};

const TileVisual kLayerVisuals[] = {
    {' ', "DeepWater", {70, 120, 190, 255}, {8, 24, 64, 255}},
    {' ', "ShallowWater", {100, 150, 210, 255}, {14, 40, 78, 255}},
    {'.', "SandAndShore", {244, 220, 140, 255}, {74, 56, 28, 255}},
    {'.', "Sand", {228, 204, 120, 255}, {60, 46, 24, 255}},
    {'o', "Rocks", {200, 200, 208, 255}, {40, 40, 44, 255}},
    {'T', "Trees", {130, 205, 120, 255}, {20, 48, 20, 255}},
    {'#', "Huts", {240, 170, 130, 255}, {72, 42, 28, 255}},
};

const cgul::Rgba8 kUiText{230, 236, 246, 255};
const cgul::Rgba8 kUiBg{8, 15, 25, 255};
const cgul::Rgba8 kPanelBg{15, 30, 48, 255};
const cgul::Rgba8 kCanvasBg{10, 10, 12, 255};
const cgul::Rgba8 kBorder{122, 170, 215, 255};
const cgul::Rgba8 kAccent{160, 210, 250, 255};

const tiled::TiledLayer* FindLayerByName(const tiled::TiledMap& map, const char* name) {
    for (std::vector<tiled::TiledLayer>::const_iterator it = map.layers.begin(); it != map.layers.end(); ++it) {
        if (it->name == name && it->isTileLayer && it->visible) {
            return &(*it);
        }
    }
    return nullptr;
}

TileLookup LookupTile(const tiled::TiledMap& map, int tileX, int tileY) {
    TileLookup lookup;
    lookup.hasTile = false;
    lookup.visual = {' ', "", {180, 180, 180, 255}, {18, 18, 22, 255}};

    if (tileX < 0 || tileY < 0 || tileX >= map.width || tileY >= map.height) {
        return lookup;
    }

    const size_t index = static_cast<size_t>(tileY * map.width + tileX);
    for (size_t i = sizeof(kLayerVisuals) / sizeof(kLayerVisuals[0]); i > 0; --i) {
        const TileVisual& visual = kLayerVisuals[i - 1];
        const tiled::TiledLayer* layer = FindLayerByName(map, visual.layerName);
        if (!layer || index >= layer->gids.size() || layer->gids[index] == 0) {
            continue;
        }
        lookup.hasTile = true;
        lookup.visual = visual;
        return lookup;
    }

    return lookup;
}

std::string FindTopLayerName(const tiled::TiledMap& map, int tileX, int tileY) {
    if (tileX < 0 || tileY < 0 || tileX >= map.width || tileY >= map.height) {
        return std::string();
    }

    const size_t index = static_cast<size_t>(tileY * map.width + tileX);
    for (std::vector<tiled::TiledLayer>::const_reverse_iterator it = map.layers.rbegin();
         it != map.layers.rend(); ++it) {
        if (!it->isTileLayer || !it->visible) {
            continue;
        }
        if (index >= it->gids.size()) {
            continue;
        }
        if (it->gids[index] != 0) {
            return it->name;
        }
    }

    return std::string();
}

void FillRect(cgul::Frame* frame, int x, int y, int w, int h, char glyph, const cgul::Rgba8& fg,
    const cgul::Rgba8& bg) {
    if (!frame || w <= 0 || h <= 0) {
        return;
    }
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(frame->width, x + w);
    const int y1 = std::min(frame->height, y + h);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            cgul::Cell& cell = frame->at(px, py);
            cell.glyph = static_cast<unsigned char>(glyph);
            cell.fg = fg;
            cell.bg = bg;
        }
    }
}

void PutText(cgul::Frame* frame, int x, int y, const std::string& text, const cgul::Rgba8& fg,
    const cgul::Rgba8& bg) {
    if (!frame || text.empty() || y < 0 || y >= frame->height) {
        return;
    }

    int textStart = 0;
    int dstX = x;
    if (dstX < 0) {
        textStart = -dstX;
        dstX = 0;
    }

    for (size_t i = static_cast<size_t>(textStart); i < text.size(); ++i) {
        const int px = dstX + static_cast<int>(i) - textStart;
        if (px < 0 || px >= frame->width) {
            continue;
        }
        cgul::Cell& cell = frame->at(px, y);
        cell.glyph = static_cast<unsigned char>(text[i]);
        cell.fg = fg;
        cell.bg = bg;
    }
}

void DrawBox(cgul::Frame* frame, int x, int y, int w, int h, const std::string& title,
    const cgul::Rgba8& borderFg, const cgul::Rgba8& panelBg) {
    if (!frame || w < 2 || h < 2) {
        return;
    }

    FillRect(frame, x, y, w, h, ' ', kUiText, panelBg);
    for (int px = x; px < x + w; ++px) {
        if (px < 0 || px >= frame->width) {
            continue;
        }
        if (y >= 0 && y < frame->height) {
            cgul::Cell& top = frame->at(px, y);
            top.glyph = '-';
            top.fg = borderFg;
            top.bg = panelBg;
        }
        if (y + h - 1 >= 0 && y + h - 1 < frame->height) {
            cgul::Cell& bottom = frame->at(px, y + h - 1);
            bottom.glyph = '-';
            bottom.fg = borderFg;
            bottom.bg = panelBg;
        }
    }
    for (int py = y; py < y + h; ++py) {
        if (py < 0 || py >= frame->height) {
            continue;
        }
        if (x >= 0 && x < frame->width) {
            cgul::Cell& left = frame->at(x, py);
            left.glyph = '|';
            left.fg = borderFg;
            left.bg = panelBg;
        }
        if (x + w - 1 >= 0 && x + w - 1 < frame->width) {
            cgul::Cell& right = frame->at(x + w - 1, py);
            right.glyph = '|';
            right.fg = borderFg;
            right.bg = panelBg;
        }
    }

    if (x >= 0 && x < frame->width && y >= 0 && y < frame->height) {
        frame->at(x, y).glyph = '+';
    }
    if (x + w - 1 >= 0 && x + w - 1 < frame->width && y >= 0 && y < frame->height) {
        frame->at(x + w - 1, y).glyph = '+';
    }
    if (x >= 0 && x < frame->width && y + h - 1 >= 0 && y + h - 1 < frame->height) {
        frame->at(x, y + h - 1).glyph = '+';
    }
    if (x + w - 1 >= 0 && x + w - 1 < frame->width && y + h - 1 >= 0 && y + h - 1 < frame->height) {
        frame->at(x + w - 1, y + h - 1).glyph = '+';
    }

    if (!title.empty() && w > 6) {
        PutText(frame, x + 2, y, "[" + title + "]", borderFg, panelBg);
    }
}

std::string ClipLine(const std::string& text, int maxCols) {
    if (maxCols <= 0) {
        return std::string();
    }
    if (static_cast<int>(text.size()) <= maxCols) {
        return text;
    }
    if (maxCols <= 3) {
        return text.substr(0, static_cast<size_t>(maxCols));
    }
    return text.substr(0, static_cast<size_t>(maxCols - 3)) + "...";
}

ImU32 ToImU32(const cgul::Rgba8& c) {
    return IM_COL32(c.r, c.g, c.b, c.a);
}

void DrawFrameToImGui(const cgul::Frame& frame, const ImVec2& origin, float cellW, float cellH) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const ImVec2 clipMax(origin.x + cellW * static_cast<float>(frame.width),
        origin.y + cellH * static_cast<float>(frame.height));
    drawList->PushClipRect(origin, clipMax, true);

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const cgul::Cell& cell = frame.at(x, y);
            const float px = origin.x + static_cast<float>(x) * cellW;
            const float py = origin.y + static_cast<float>(y) * cellH;
            drawList->AddRectFilled(ImVec2(px, py), ImVec2(px + cellW, py + cellH), ToImU32(cell.bg));

            if (cell.glyph == U' ') {
                continue;
            }
            char glyph[2] = {'?', '\0'};
            if (cell.glyph >= 32 && cell.glyph <= 126) {
                glyph[0] = static_cast<char>(cell.glyph);
            }
            drawList->AddText(font, fontSize, ImVec2(px, py), ToImU32(cell.fg), glyph);
        }
    }

    drawList->PopClipRect();
}

}  // namespace

void CgulUiRenderer::Draw(WorldState* worldState, const tools::ChunkExporterTool* tool) {
    if (!worldState || !tool) {
        ImGui::TextUnformatted("CGUL UI unavailable.");
        return;
    }

    ImFont* font = ImGui::GetIO().FontDefault;
    if (font) {
        ImGui::PushFont(font);
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x <= 1.0f || available.y <= 1.0f) {
        if (font) {
            ImGui::PopFont();
        }
        return;
    }

    const float cellW = std::max(1.0f, ImGui::CalcTextSize("M").x);
    const float cellH = std::max(1.0f, ImGui::GetTextLineHeightWithSpacing());
    const int frameW = std::max(1, std::min(220, static_cast<int>(std::floor(available.x / cellW))));
    const int frameH = std::max(1, std::min(120, static_cast<int>(std::floor(available.y / cellH))));
    cgul::Frame frame(frameW, frameH);

    FillRect(&frame, 0, 0, frameW, frameH, ' ', kUiText, kUiBg);

    const int topH = std::min(3, frameH);
    const int contentY = topH;
    const int contentH = std::max(0, frameH - contentY);
    int rightW = std::clamp(56, 32, std::max(32, frameW - 24));
    if (contentH < 4 || frameW < 56) {
        rightW = std::max(0, frameW / 3);
    }
    int leftW = frameW - rightW;
    if (leftW < 24) {
        rightW = 0;
        leftW = frameW;
    }

    DrawBox(&frame, 0, 0, frameW, std::max(2, topH), "CGUL UI", kBorder, kPanelBg);
    PutText(&frame, 2, 1, "TAB: Art Mode (CGUL UI)", kAccent, kPanelBg);
    PutText(&frame, 27, 1, "RMB drag / Arrows pan / +/- zoom / [] fine / 0 reset / Wheel zoom", kUiText,
        kPanelBg);

    DrawBox(&frame, 0, contentY, leftW, contentH, "Viewport", kBorder, kPanelBg);
    if (rightW > 0) {
        DrawBox(&frame, leftW, contentY, rightW, contentH, "Chunk Exporter", kBorder, kPanelBg);
    }

    const int viewX = 1;
    const int viewY = contentY + 1;
    const int viewW = std::max(0, leftW - 2);
    const int viewH = std::max(0, contentH - 2);
    constexpr int headerRows = 2;
    const int mapAreaX = viewX;
    const int mapAreaY = viewY + headerRows;
    const int mapAreaW = viewW;
    const int mapAreaH = std::max(0, viewH - headerRows);

    int fallbackCanvasW = mapAreaW;
    int fallbackCanvasH = mapAreaH;
    if (mapAreaW > 0 && mapAreaH > 0) {
        const int canvasSize = std::min(mapAreaW, mapAreaH);
        if (canvasSize >= 8) {
            fallbackCanvasW = canvasSize;
            fallbackCanvasH = canvasSize;
        }
    }

    int canvasX = mapAreaX;
    int canvasY = mapAreaY;
    int canvasW = mapAreaW;
    int canvasH = mapAreaH;
    if (mapAreaW > 0 && mapAreaH > 0) {
        const float mapAreaWpx = static_cast<float>(mapAreaW) * cellW;
        const float mapAreaHpx = static_cast<float>(mapAreaH) * cellH;
        const float squarePx = std::floor(std::min(mapAreaWpx, mapAreaHpx));
        int pixelSquareW = static_cast<int>(std::floor(squarePx / cellW));
        int pixelSquareH = static_cast<int>(std::floor(squarePx / cellH));
        pixelSquareW = std::max(0, std::min(pixelSquareW, mapAreaW));
        pixelSquareH = std::max(0, std::min(pixelSquareH, mapAreaH));

        if (pixelSquareW >= 8 && pixelSquareH >= 8) {
            canvasW = pixelSquareW;
            canvasH = pixelSquareH;
        } else {
            canvasW = fallbackCanvasW;
            canvasH = fallbackCanvasH;
        }

        canvasX = mapAreaX + (mapAreaW - canvasW) / 2;
        canvasY = mapAreaY + (mapAreaH - canvasH) / 2;
    }

    if (mapAreaW > 0 && mapAreaH > 0) {
        FillRect(&frame, mapAreaX, mapAreaY, mapAreaW, mapAreaH, ' ', kUiText, kPanelBg);
    }

    auto drawCanvasBorder = [&frame, canvasX, canvasY, canvasW, canvasH]() {
        for (int px = canvasX; px < canvasX + canvasW; ++px) {
            if (px < 0 || px >= frame.width) {
                continue;
            }
            if (canvasY >= 0 && canvasY < frame.height) {
                cgul::Cell& top = frame.at(px, canvasY);
                top.glyph = '-';
                top.fg = kBorder;
                top.bg = kCanvasBg;
            }
            if (canvasY + canvasH - 1 >= 0 && canvasY + canvasH - 1 < frame.height) {
                cgul::Cell& bottom = frame.at(px, canvasY + canvasH - 1);
                bottom.glyph = '-';
                bottom.fg = kBorder;
                bottom.bg = kCanvasBg;
            }
        }
        for (int py = canvasY; py < canvasY + canvasH; ++py) {
            if (py < 0 || py >= frame.height) {
                continue;
            }
            if (canvasX >= 0 && canvasX < frame.width) {
                cgul::Cell& left = frame.at(canvasX, py);
                left.glyph = '|';
                left.fg = kBorder;
                left.bg = kCanvasBg;
            }
            if (canvasX + canvasW - 1 >= 0 && canvasX + canvasW - 1 < frame.width) {
                cgul::Cell& right = frame.at(canvasX + canvasW - 1, py);
                right.glyph = '|';
                right.fg = kBorder;
                right.bg = kCanvasBg;
            }
        }
        if (canvasX >= 0 && canvasX < frame.width && canvasY >= 0 && canvasY < frame.height) {
            frame.at(canvasX, canvasY).glyph = '+';
        }
        if (canvasX + canvasW - 1 >= 0 && canvasX + canvasW - 1 < frame.width && canvasY >= 0 &&
            canvasY < frame.height) {
            frame.at(canvasX + canvasW - 1, canvasY).glyph = '+';
        }
        if (canvasX >= 0 && canvasX < frame.width && canvasY + canvasH - 1 >= 0 &&
            canvasY + canvasH - 1 < frame.height) {
            frame.at(canvasX, canvasY + canvasH - 1).glyph = '+';
        }
        if (canvasX + canvasW - 1 >= 0 && canvasX + canvasW - 1 < frame.width &&
            canvasY + canvasH - 1 >= 0 && canvasY + canvasH - 1 < frame.height) {
            frame.at(canvasX + canvasW - 1, canvasY + canvasH - 1).glyph = '+';
        }
    };

    if (canvasW > 0 && canvasH > 0) {
        FillRect(&frame, canvasX, canvasY, canvasW, canvasH, ' ', kUiText, kCanvasBg);
        drawCanvasBorder();
    }

    const bool hasMap = tool->HasMap();
    worldState->hasMap = hasMap;
    if (hasMap) {
        worldState->map = tool->GetMap();
    }

    float visibleW = 1.0f;
    float visibleH = 1.0f;
    if (hasMap && worldState->map.width > 0 && worldState->map.height > 0 && canvasW > 0 && canvasH > 0) {
        worldState->ClampCameraToMap();

        const float mapWidth = static_cast<float>(worldState->map.width);
        const float mapHeight = static_cast<float>(worldState->map.height);
        visibleW = std::max(1.0f, mapWidth / worldState->zoom);
        visibleH = std::max(1.0f, mapHeight / worldState->zoom);

        for (int y = 0; y < canvasH; ++y) {
            for (int x = 0; x < canvasW; ++x) {
                const float tx = worldState->cameraTileX +
                    (visibleW * (static_cast<float>(x) + 0.5f) / static_cast<float>(canvasW));
                const float ty = worldState->cameraTileY +
                    (visibleH * (static_cast<float>(y) + 0.5f) / static_cast<float>(canvasH));
                const int mapX = std::max(0, std::min(static_cast<int>(tx), worldState->map.width - 1));
                const int mapY = std::max(0, std::min(static_cast<int>(ty), worldState->map.height - 1));
                const TileLookup lookup = LookupTile(worldState->map, mapX, mapY);

                cgul::Cell& cell = frame.at(canvasX + x, canvasY + y);
                if (lookup.hasTile) {
                    cell.glyph = static_cast<unsigned char>(lookup.visual.glyph);
                    cell.fg = lookup.visual.fg;
                    cell.bg = lookup.visual.bg;
                } else {
                    cell.glyph = ' ';
                    cell.fg = {120, 120, 120, 255};
                    cell.bg = {14, 14, 18, 255};
                }
            }
        }
        drawCanvasBorder();

        char line0[128];
        std::snprintf(line0, sizeof(line0), "Map %dx%d  tile %dx%d", worldState->map.width, worldState->map.height,
            worldState->map.tileWidth, worldState->map.tileHeight);
        PutText(&frame, viewX, viewY, ClipLine(line0, viewW), kUiText, kPanelBg);

        char line1[128];
        std::snprintf(line1, sizeof(line1), "Camera %.1f, %.1f  zoom %.2f", worldState->cameraTileX,
            worldState->cameraTileY, worldState->zoom);
        if (viewH > 1) {
            PutText(&frame, viewX, viewY + 1, ClipLine(line1, viewW), kAccent, kPanelBg);
        }
    } else {
        PutText(&frame, mapAreaX, mapAreaY, "Load a map in default mode, then press TAB.", kUiText, kPanelBg);
    }

    if (rightW > 0) {
        const int panelX = leftW + 1;
        const int panelY = contentY + 1;
        const int panelW = std::max(0, rightW - 2);
        const int panelH = std::max(0, contentH - 2);
        std::vector<std::string> lines;
        lines.reserve(14);
        lines.push_back("read-only snapshot");
        lines.push_back("input: " + tool->GetInputPath());
        lines.push_back(hasMap ? "hasMap: true" : "hasMap: false");
        if (hasMap) {
            char mapLine[96];
            std::snprintf(mapLine, sizeof(mapLine), "map: %dx%d tile %dx%d", tool->GetMap().width,
                tool->GetMap().height, tool->GetMap().tileWidth, tool->GetMap().tileHeight);
            lines.emplace_back(mapLine);
        }
        lines.push_back("chunkType: " + tool->GetChunkType());
        char chunkLine[96];
        std::snprintf(chunkLine, sizeof(chunkLine), "chunk: %d x %d tiles", tool->GetChunkWidthTiles(),
            tool->GetChunkHeightTiles());
        lines.emplace_back(chunkLine);
        char tileLine[96];
        std::snprintf(tileLine, sizeof(tileLine), "tileSizePx: %d", tool->GetSelectedTileSizePx());
        lines.emplace_back(tileLine);
        lines.push_back(std::string("nonEmptyOnly: ") + (tool->GetExportNonEmptyOnly() ? "true" : "false"));
        lines.push_back("outputRoot:");
        lines.push_back(tool->GetOutputRoot().string());
        if (!worldState->statusText.empty()) {
            lines.push_back("status:");
            lines.push_back(worldState->statusText);
        }
        if (!worldState->errorText.empty()) {
            lines.push_back("error:");
            lines.push_back(worldState->errorText);
        }

        const int maxLines = std::min(panelH, static_cast<int>(lines.size()));
        for (int i = 0; i < maxLines; ++i) {
            const bool keyLine = i == 0;
            PutText(&frame, panelX, panelY + i, ClipLine(lines[static_cast<size_t>(i)], panelW),
                keyLine ? kAccent : kUiText, kPanelBg);
        }
    }

    const ImVec2 frameOrigin = ImGui::GetCursorScreenPos();
    DrawFrameToImGui(frame, frameOrigin, cellW, cellH);
    ImGui::Dummy(ImVec2(static_cast<float>(frameW) * cellW, static_cast<float>(frameH) * cellH));

    const float viewportX = frameOrigin.x + static_cast<float>(canvasX) * cellW;
    const float viewportY = frameOrigin.y + static_cast<float>(canvasY) * cellH;
    const float viewportWpx = static_cast<float>(canvasW) * cellW;
    const float viewportHpx = static_cast<float>(canvasH) * cellH;
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool inViewport = viewportWpx > 0.0f && viewportHpx > 0.0f && mouse.x >= viewportX &&
        mouse.x <= viewportX + viewportWpx && mouse.y >= viewportY && mouse.y <= viewportY + viewportHpx;

    if (hasMap && worldState->map.width > 0 && worldState->map.height > 0 && viewportWpx > 0.0f &&
        viewportHpx > 0.0f) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 clipMin(viewportX, viewportY);
        const ImVec2 clipMax(viewportX + viewportWpx, viewportY + viewportHpx);
        dl->PushClipRect(clipMin, clipMax, true);

        const int chunkW = tool->GetChunkWidthTiles();
        const int chunkH = tool->GetChunkHeightTiles();
        const ImU32 magenta = IM_COL32(255, 0, 255, 180);
        const ImU32 blue = IM_COL32(30, 160, 255, 180);

        if (chunkW > 0 && visibleW > 0.0f) {
            for (int tileX = 0; tileX <= worldState->map.width; tileX += chunkW) {
                const float t = (static_cast<float>(tileX) - worldState->cameraTileX) / visibleW;
                if (t < 0.0f || t > 1.0f) {
                    continue;
                }
                const float px = viewportX + t * viewportWpx;
                dl->AddLine(ImVec2(px, viewportY), ImVec2(px, viewportY + viewportHpx), magenta);
            }
        }

        if (chunkH > 0 && visibleH > 0.0f) {
            for (int tileY = 0; tileY <= worldState->map.height; tileY += chunkH) {
                const float t = (static_cast<float>(tileY) - worldState->cameraTileY) / visibleH;
                if (t < 0.0f || t > 1.0f) {
                    continue;
                }
                const float py = viewportY + t * viewportHpx;
                dl->AddLine(ImVec2(viewportX, py), ImVec2(viewportX + viewportWpx, py), blue);
            }
        }

        dl->PopClipRect();
    }

    worldState->viewportScreenX = viewportX;
    worldState->viewportScreenY = viewportY;
    worldState->viewportWidthPx = viewportWpx;
    worldState->viewportHeightPx = viewportHpx;
    worldState->visibleTileSpanX = visibleW;
    worldState->visibleTileSpanY = visibleH;
    worldState->viewportHovered = hasMap && inViewport;

    if (hasMap && inViewport && viewportWpx > 0.0f && viewportHpx > 0.0f) {
        const float relX = std::max(0.0f, std::min(1.0f, (mouse.x - viewportX) / viewportWpx));
        const float relY = std::max(0.0f, std::min(1.0f, (mouse.y - viewportY) / viewportHpx));
        int hoverX = static_cast<int>(std::floor(worldState->cameraTileX + relX * visibleW));
        int hoverY = static_cast<int>(std::floor(worldState->cameraTileY + relY * visibleH));
        hoverX = std::max(0, std::min(hoverX, worldState->map.width - 1));
        hoverY = std::max(0, std::min(hoverY, worldState->map.height - 1));
        worldState->hoverTileX = hoverX;
        worldState->hoverTileY = hoverY;
        worldState->hoverLayerName = FindTopLayerName(worldState->map, hoverX, hoverY);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            worldState->selectedTileX = hoverX;
            worldState->selectedTileY = hoverY;
        }
    }

    if (font) {
        ImGui::PopFont();
    }
}

}  // namespace cgul_demo
