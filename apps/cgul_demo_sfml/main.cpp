#include "cgul/core/equality.h"
#include "cgul/io/cgul_document.h"
#include "cgul/validate/validate.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kDefaultGridW = 60;
constexpr int kDefaultGridH = 30;
constexpr int kCellSizePx = 16;
constexpr int kTopBarPx = 42;
constexpr int kMinWindowW = 10;
constexpr int kMinWindowH = 6;
constexpr int kInitialWindowCount = 5;

struct CliOptions {
  uint32_t seed = 42;
  int windowCount = kInitialWindowCount;
  std::string savePath = "demo_layout.cgul";
  std::optional<std::string> startupSavePath;
  std::optional<std::string> startupLoadPath;
  bool exitAfterStartup = false;
};

enum class EditMode {
  None,
  Drag,
  Resize,
};

struct EditState {
  EditMode mode = EditMode::None;
  uint32_t widgetId = 0;
  int dragOffsetX = 0;
  int dragOffsetY = 0;
};

bool ParseU32(const std::string& text, uint32_t* outValue) {
  if (outValue == nullptr || text.empty()) {
    return false;
  }

  uint64_t value = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    const uint64_t digit = static_cast<uint64_t>(ch - '0');
    if (value > (UINT32_MAX - digit) / 10ULL) {
      return false;
    }
    value = value * 10ULL + digit;
  }

  *outValue = static_cast<uint32_t>(value);
  return true;
}

bool ParseInt(const std::string& text, int* outValue) {
  if (outValue == nullptr || text.empty()) {
    return false;
  }

  bool negative = false;
  size_t i = 0;
  if (text[0] == '-') {
    negative = true;
    i = 1;
  }
  if (i >= text.size()) {
    return false;
  }

  int64_t value = 0;
  for (; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch < '0' || ch > '9') {
      return false;
    }
    value = value * 10 + static_cast<int64_t>(ch - '0');
    if (value > INT32_MAX) {
      return false;
    }
  }

  if (negative) {
    value = -value;
  }

  if (value < INT32_MIN || value > INT32_MAX) {
    return false;
  }

  *outValue = static_cast<int>(value);
  return true;
}

bool ParseArgs(int argc, char** argv, CliOptions* outOptions, std::string* outError) {
  if (outOptions == nullptr) {
    if (outError != nullptr) {
      *outError = "internal parser error";
    }
    return false;
  }

  CliOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--seed") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--seed requires a value";
        }
        return false;
      }
      if (!ParseU32(argv[i + 1], &options.seed)) {
        if (outError != nullptr) {
          *outError = "--seed must be a valid u32";
        }
        return false;
      }
      ++i;
      continue;
    }

    if (arg == "--windows") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--windows requires a value";
        }
        return false;
      }
      if (!ParseInt(argv[i + 1], &options.windowCount) || options.windowCount <= 0) {
        if (outError != nullptr) {
          *outError = "--windows must be a positive integer";
        }
        return false;
      }
      ++i;
      continue;
    }

    if (arg == "--save") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--save requires a path";
        }
        return false;
      }
      options.startupSavePath = argv[++i];
      continue;
    }

    if (arg == "--load") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--load requires a path";
        }
        return false;
      }
      options.startupLoadPath = argv[++i];
      continue;
    }

    if (arg == "--exit-after-startup") {
      options.exitAfterStartup = true;
      continue;
    }

    if (arg == "--help" || arg == "-h") {
      std::cout
          << "cgul_demo_sfml options:\n"
          << "  --seed <u32>            Initial deterministic seed (default: 42)\n"
          << "  --windows <N>           Initial desired window count (default: 5)\n"
          << "  --save <path>           Save document on startup\n"
          << "  --load <path>           Load document on startup\n"
          << "  --exit-after-startup    Exit after startup load/save actions\n";
      return false;
    }

    if (outError != nullptr) {
      *outError = "unknown argument: " + arg;
    }
    return false;
  }

  *outOptions = options;
  return true;
}

bool RectanglesOverlap(const cgul::RectI& a, const cgul::RectI& b) {
  return a.x < (b.x + b.w) && b.x < (a.x + a.w) && a.y < (b.y + b.h) && b.y < (a.y + a.h);
}

bool ContainsCell(const cgul::RectI& rect, int cellX, int cellY) {
  return cellX >= rect.x && cellY >= rect.y && cellX < (rect.x + rect.w) && cellY < (rect.y + rect.h);
}

bool IsTitleBarCell(const cgul::RectI& rect, int cellX, int cellY) {
  return ContainsCell(rect, cellX, cellY) && cellY == rect.y;
}

bool IsResizeHandleCell(const cgul::RectI& rect, int cellX, int cellY) {
  const int rx = rect.x + rect.w - 1;
  const int ry = rect.y + rect.h - 1;
  return cellX >= rx - 1 && cellX <= rx && cellY >= ry - 1 && cellY <= ry;
}

cgul::Widget* FindWidgetById(cgul::CgulDocument* doc, uint32_t id) {
  if (doc == nullptr) {
    return nullptr;
  }
  for (cgul::Widget& widget : doc->widgets) {
    if (widget.id == id) {
      return &widget;
    }
  }
  return nullptr;
}

const cgul::Widget* FindWidgetById(const cgul::CgulDocument& doc, uint32_t id) {
  for (const cgul::Widget& widget : doc.widgets) {
    if (widget.id == id) {
      return &widget;
    }
  }
  return nullptr;
}

uint32_t FindTopWidgetAtCell(const cgul::CgulDocument& doc, int cellX, int cellY) {
  for (auto it = doc.widgets.rbegin(); it != doc.widgets.rend(); ++it) {
    if (it->kind != cgul::WidgetKind::Window) {
      continue;
    }
    if (ContainsCell(it->boundsCells, cellX, cellY)) {
      return it->id;
    }
  }
  return 0;
}

bool IsValidDoc(const cgul::CgulDocument& doc, const std::string& context) {
  std::string error;
  if (!cgul::Validate(doc, &error)) {
    std::cerr << context << " validation failed: " << error << "\n";
    return false;
  }
  return true;
}

std::optional<cgul::CgulDocument> GenerateDeterministicLayout(uint32_t seed, int desiredWindowCount,
                                                              int gridW, int gridH) {
  cgul::CgulDocument best;
  best.cgulVersion = "0.1";
  best.gridWCells = gridW;
  best.gridHCells = gridH;
  best.seed = seed;

  std::mt19937_64 rng(seed);

  int targetCount = std::max(1, desiredWindowCount);

  while (targetCount >= 1) {
    cgul::CgulDocument doc;
    doc.cgulVersion = "0.1";
    doc.gridWCells = gridW;
    doc.gridHCells = gridH;
    doc.seed = seed;

    bool allPlaced = true;
    for (int i = 0; i < targetCount; ++i) {
      const uint32_t id = static_cast<uint32_t>(i + 1);

      bool placed = false;
      cgul::RectI placedRect{};

      for (int attempt = 0; attempt < 1500 && !placed; ++attempt) {
        std::uniform_int_distribution<int> widthDist(kMinWindowW, std::min(24, gridW));
        std::uniform_int_distribution<int> heightDist(kMinWindowH, std::min(14, gridH));

        const int w = widthDist(rng);
        const int h = heightDist(rng);

        if (w > gridW || h > gridH) {
          continue;
        }

        std::uniform_int_distribution<int> xDist(0, gridW - w);
        std::uniform_int_distribution<int> yDist(0, gridH - h);

        const cgul::RectI candidate{xDist(rng), yDist(rng), w, h};

        bool overlap = false;
        for (const cgul::Widget& existing : doc.widgets) {
          if (RectanglesOverlap(candidate, existing.boundsCells)) {
            overlap = true;
            break;
          }
        }

        if (!overlap) {
          placedRect = candidate;
          placed = true;
        }
      }

      if (!placed) {
        allPlaced = false;
        break;
      }

      cgul::Widget widget;
      widget.id = id;
      widget.kind = cgul::WidgetKind::Window;
      widget.boundsCells = placedRect;
      widget.title = "Window " + std::to_string(id);
      doc.widgets.push_back(std::move(widget));
    }

    if (allPlaced) {
      std::string error;
      if (cgul::Validate(doc, &error)) {
        return doc;
      }
    }

    --targetCount;
  }

  return std::nullopt;
}

bool SaveWithRoundTripCheck(const std::string& path, const cgul::CgulDocument& doc) {
  std::string error;
  if (!cgul::Validate(doc, &error)) {
    std::cerr << "Save failed validation: " << error << "\n";
    return false;
  }

  if (!cgul::SaveCgulFile(path, doc, &error)) {
    std::cerr << "Save failed: " << error << "\n";
    return false;
  }

  cgul::CgulDocument reloaded;
  if (!cgul::LoadCgulFile(path, &reloaded, &error)) {
    std::cerr << "Round-trip reload failed: " << error << "\n";
    return false;
  }

  if (!cgul::Validate(reloaded, &error)) {
    std::cerr << "Round-trip validate failed: " << error << "\n";
    return false;
  }

  std::string diff;
  if (!cgul::Equal(doc, reloaded, &diff)) {
    std::cerr << "Round-trip equality FAIL: " << diff << "\n";
    return false;
  }

  std::cout << "Round-trip PASS: " << path << "\n";
  return true;
}

bool LoadDocumentFile(const std::string& path, cgul::CgulDocument* outDoc) {
  if (outDoc == nullptr) {
    return false;
  }

  std::string error;
  cgul::CgulDocument loaded;
  if (!cgul::LoadCgulFile(path, &loaded, &error)) {
    std::cerr << "Load failed: " << error << "\n";
    return false;
  }

  if (!cgul::Validate(loaded, &error)) {
    std::cerr << "Load validation failed: " << error << "\n";
    return false;
  }

  *outDoc = std::move(loaded);
  std::cout << "Loaded: " << path << "\n";
  return true;
}

bool PixelToCell(const sf::Vector2i& pixel, int* outCellX, int* outCellY) {
  if (outCellX == nullptr || outCellY == nullptr) {
    return false;
  }

  if (pixel.y < kTopBarPx) {
    return false;
  }

  const int cx = pixel.x / kCellSizePx;
  const int cy = (pixel.y - kTopBarPx) / kCellSizePx;
  if (cx < 0 || cy < 0) {
    return false;
  }

  *outCellX = cx;
  *outCellY = cy;
  return true;
}

sf::FloatRect WindowPixelRect(const cgul::RectI& rect) {
  return sf::FloatRect(
      {static_cast<float>(rect.x * kCellSizePx), static_cast<float>(kTopBarPx + rect.y * kCellSizePx)},
      {static_cast<float>(rect.w * kCellSizePx), static_cast<float>(rect.h * kCellSizePx)});
}

bool TryLoadFont(sf::Font* outFont, std::string* outLoadedPath) {
  if (outFont == nullptr) {
    return false;
  }

  const std::vector<fs::path> candidates = {
      fs::path("assets") / "fonts" / "DejaVuSansMono.ttf",
      fs::path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
  };

  for (const fs::path& candidate : candidates) {
    if (!fs::exists(candidate)) {
      continue;
    }
    if (outFont->openFromFile(candidate.string())) {
      if (outLoadedPath != nullptr) {
        *outLoadedPath = candidate.string();
      }
      return true;
    }
  }

  return false;
}

void DrawGrid(sf::RenderWindow& window, int gridW, int gridH) {
  sf::VertexArray lines(sf::PrimitiveType::Lines);
  const sf::Color gridColor(55, 55, 60);

  for (int x = 0; x <= gridW; ++x) {
    const float px = static_cast<float>(x * kCellSizePx);
    lines.append(sf::Vertex({px, static_cast<float>(kTopBarPx)}, gridColor));
    lines.append(sf::Vertex({px, static_cast<float>(kTopBarPx + gridH * kCellSizePx)}, gridColor));
  }

  for (int y = 0; y <= gridH; ++y) {
    const float py = static_cast<float>(kTopBarPx + y * kCellSizePx);
    lines.append(sf::Vertex({0.f, py}, gridColor));
    lines.append(sf::Vertex({static_cast<float>(gridW * kCellSizePx), py}, gridColor));
  }

  window.draw(lines);
}

void DrawText(sf::RenderWindow& window, const sf::Font* font, const std::string& text, float x, float y,
              unsigned int size, const sf::Color& color) {
  if (font == nullptr) {
    return;
  }

  sf::Text drawable(*font, text, size);
  drawable.setPosition({x, y});
  drawable.setFillColor(color);
  window.draw(drawable);
}

void DrawDocument(sf::RenderWindow& window, const cgul::CgulDocument& doc, const sf::Font* font,
                  uint32_t hoveredId, uint32_t activeId) {
  for (const cgul::Widget& widget : doc.widgets) {
    const sf::FloatRect pxRect = WindowPixelRect(widget.boundsCells);

    sf::RectangleShape body({pxRect.size.x, pxRect.size.y});
    body.setPosition({pxRect.position.x, pxRect.position.y});
    body.setFillColor(sf::Color(45, 52, 66));
    body.setOutlineThickness(1.f);
    body.setOutlineColor(sf::Color(160, 180, 210));

    if (widget.id == hoveredId) {
      body.setOutlineColor(sf::Color(210, 220, 100));
      body.setOutlineThickness(2.f);
    }
    if (widget.id == activeId) {
      body.setOutlineColor(sf::Color(80, 220, 170));
      body.setOutlineThickness(3.f);
    }

    window.draw(body);

    sf::RectangleShape titleBar({pxRect.size.x, static_cast<float>(kCellSizePx)});
    titleBar.setPosition({pxRect.position.x, pxRect.position.y});
    titleBar.setFillColor(sf::Color(60, 75, 110));
    window.draw(titleBar);

    const std::string title = widget.title.empty() ? ("Window " + std::to_string(widget.id)) : widget.title;
    DrawText(window, font, title, pxRect.position.x + 4.f, pxRect.position.y + 1.f, 14, sf::Color::White);

    const std::string sizeText = "W x H: " + std::to_string(widget.boundsCells.w) + " x " +
                                 std::to_string(widget.boundsCells.h) + "  pos: " +
                                 std::to_string(widget.boundsCells.x) + "," +
                                 std::to_string(widget.boundsCells.y);

    DrawText(window, font, sizeText, pxRect.position.x + 4.f,
             pxRect.position.y + static_cast<float>(kCellSizePx + 2), 13, sf::Color(230, 230, 210));

    sf::RectangleShape resizeHandle({static_cast<float>(kCellSizePx), static_cast<float>(kCellSizePx)});
    resizeHandle.setPosition({pxRect.position.x + pxRect.size.x - static_cast<float>(kCellSizePx),
                              pxRect.position.y + pxRect.size.y - static_cast<float>(kCellSizePx)});
    resizeHandle.setFillColor(sf::Color(120, 120, 120));
    window.draw(resizeHandle);
  }
}

bool ApplyCandidateIfValid(cgul::CgulDocument* doc, const cgul::CgulDocument& candidate) {
  if (doc == nullptr) {
    return false;
  }

  std::string error;
  if (!cgul::Validate(candidate, &error)) {
    return false;
  }

  *doc = candidate;
  return true;
}

int RunApp(const CliOptions& options) {
  cgul::CgulDocument doc;

  uint32_t currentSeed = options.seed;
  int desiredWindowCount = std::max(1, options.windowCount);
  std::string activeSavePath = options.savePath;

  if (options.startupLoadPath.has_value()) {
    activeSavePath = *options.startupLoadPath;
    if (!LoadDocumentFile(*options.startupLoadPath, &doc)) {
      return 1;
    }
    currentSeed = static_cast<uint32_t>(doc.seed);
  } else {
    const auto generated =
        GenerateDeterministicLayout(currentSeed, desiredWindowCount, kDefaultGridW, kDefaultGridH);
    if (!generated.has_value()) {
      std::cerr << "Unable to generate a valid initial layout\n";
      return 1;
    }
    doc = *generated;
  }

  if (options.startupSavePath.has_value()) {
    activeSavePath = *options.startupSavePath;
    if (!SaveWithRoundTripCheck(*options.startupSavePath, doc)) {
      return 1;
    }
  }

  if (options.exitAfterStartup) {
    return 0;
  }

  const unsigned int windowW = static_cast<unsigned int>(doc.gridWCells * kCellSizePx);
  const unsigned int windowH = static_cast<unsigned int>(kTopBarPx + doc.gridHCells * kCellSizePx);

  sf::RenderWindow window(sf::VideoMode({windowW, windowH}), "CGUL Demo SFML v0");
  window.setFramerateLimit(60);

  sf::Font font;
  const sf::Font* fontPtr = nullptr;
  std::string fontPath;
  if (TryLoadFont(&font, &fontPath)) {
    fontPtr = &font;
    std::cout << "Loaded font: " << fontPath << "\n";
  } else {
    std::cerr << "Warning: monospace font not found; rendering without text labels\n";
  }

  bool showGrid = false;
  EditState edit;

  while (window.isOpen()) {
    while (const std::optional event = window.pollEvent()) {
      if (event->is<sf::Event::Closed>()) {
        window.close();
        continue;
      }

      if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
        const auto scancode = keyPressed->scancode;

        if (scancode == sf::Keyboard::Scancode::Escape) {
          window.close();
          continue;
        }

        if (scancode == sf::Keyboard::Scancode::G) {
          ++currentSeed;
          const auto generated =
              GenerateDeterministicLayout(currentSeed, desiredWindowCount, doc.gridWCells, doc.gridHCells);
          if (generated.has_value()) {
            doc = *generated;
            std::cout << "Generated layout with seed " << currentSeed << " windows=" << doc.widgets.size() << "\n";
          }
          continue;
        }

        if (scancode == sf::Keyboard::Scancode::S) {
          if (SaveWithRoundTripCheck(activeSavePath, doc)) {
            std::cout << "Saved: " << activeSavePath << "\n";
          }
          continue;
        }

        if (scancode == sf::Keyboard::Scancode::L) {
          cgul::CgulDocument loaded;
          if (LoadDocumentFile(activeSavePath, &loaded)) {
            doc = std::move(loaded);
          }
          continue;
        }

        if (scancode == sf::Keyboard::Scancode::F3) {
          showGrid = !showGrid;
          continue;
        }

        if (scancode == sf::Keyboard::Scancode::Equal ||
            scancode == sf::Keyboard::Scancode::NumpadPlus) {
          ++desiredWindowCount;
          const auto generated =
              GenerateDeterministicLayout(currentSeed, desiredWindowCount, doc.gridWCells, doc.gridHCells);
          if (generated.has_value()) {
            doc = *generated;
            std::cout << "Window count " << desiredWindowCount << " seed=" << currentSeed << "\n";
          }
          continue;
        }

        if (scancode == sf::Keyboard::Scancode::Hyphen ||
            scancode == sf::Keyboard::Scancode::NumpadMinus) {
          desiredWindowCount = std::max(1, desiredWindowCount - 1);
          const auto generated =
              GenerateDeterministicLayout(currentSeed, desiredWindowCount, doc.gridWCells, doc.gridHCells);
          if (generated.has_value()) {
            doc = *generated;
            std::cout << "Window count " << desiredWindowCount << " seed=" << currentSeed << "\n";
          }
          continue;
        }
      }

      if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePressed->button != sf::Mouse::Button::Left) {
          continue;
        }

        const sf::Vector2i pixel = mousePressed->position;

        const sf::FloatRect generateButtonRect({10.f, 7.f}, {120.f, 28.f});
        if (generateButtonRect.contains(static_cast<sf::Vector2f>(pixel))) {
          ++currentSeed;
          const auto generated =
              GenerateDeterministicLayout(currentSeed, desiredWindowCount, doc.gridWCells, doc.gridHCells);
          if (generated.has_value()) {
            doc = *generated;
            std::cout << "Generated layout with seed " << currentSeed << " windows=" << doc.widgets.size() << "\n";
          }
          continue;
        }

        int cellX = 0;
        int cellY = 0;
        if (!PixelToCell(pixel, &cellX, &cellY)) {
          continue;
        }

        const uint32_t widgetId = FindTopWidgetAtCell(doc, cellX, cellY);
        const cgul::Widget* widget = FindWidgetById(doc, widgetId);
        if (widget == nullptr || widget->kind != cgul::WidgetKind::Window) {
          continue;
        }

        if (IsResizeHandleCell(widget->boundsCells, cellX, cellY)) {
          edit.mode = EditMode::Resize;
          edit.widgetId = widgetId;
          continue;
        }

        if (IsTitleBarCell(widget->boundsCells, cellX, cellY)) {
          edit.mode = EditMode::Drag;
          edit.widgetId = widgetId;
          edit.dragOffsetX = cellX - widget->boundsCells.x;
          edit.dragOffsetY = cellY - widget->boundsCells.y;
          continue;
        }
      }

      if (event->is<sf::Event::MouseButtonReleased>()) {
        edit.mode = EditMode::None;
        edit.widgetId = 0;
      }

      if (const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
        if (edit.mode == EditMode::None || edit.widgetId == 0) {
          continue;
        }

        int cellX = 0;
        int cellY = 0;
        if (!PixelToCell(moved->position, &cellX, &cellY)) {
          continue;
        }

        cgul::CgulDocument candidate = doc;
        cgul::Widget* candidateWidget = FindWidgetById(&candidate, edit.widgetId);
        if (candidateWidget == nullptr) {
          continue;
        }

        if (edit.mode == EditMode::Drag) {
          const int maxX = candidate.gridWCells - candidateWidget->boundsCells.w;
          const int maxY = candidate.gridHCells - candidateWidget->boundsCells.h;
          candidateWidget->boundsCells.x = std::clamp(cellX - edit.dragOffsetX, 0, std::max(0, maxX));
          candidateWidget->boundsCells.y =
              std::clamp(cellY - edit.dragOffsetY, 0, std::max(0, maxY));
        } else if (edit.mode == EditMode::Resize) {
          const int x = candidateWidget->boundsCells.x;
          const int y = candidateWidget->boundsCells.y;
          int newW = cellX - x + 1;
          int newH = cellY - y + 1;

          newW = std::clamp(newW, kMinWindowW, candidate.gridWCells - x);
          newH = std::clamp(newH, kMinWindowH, candidate.gridHCells - y);

          candidateWidget->boundsCells.w = newW;
          candidateWidget->boundsCells.h = newH;
        }

        ApplyCandidateIfValid(&doc, candidate);
      }
    }

    const sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    int hoverCellX = 0;
    int hoverCellY = 0;
    uint32_t hoveredId = 0;
    if (PixelToCell(mousePixel, &hoverCellX, &hoverCellY)) {
      hoveredId = FindTopWidgetAtCell(doc, hoverCellX, hoverCellY);
    }

    window.clear(sf::Color(28, 30, 35));

    sf::RectangleShape topBar({static_cast<float>(window.getSize().x), static_cast<float>(kTopBarPx)});
    topBar.setPosition({0.f, 0.f});
    topBar.setFillColor(sf::Color(20, 22, 26));
    window.draw(topBar);

    sf::RectangleShape button({120.f, 28.f});
    button.setPosition({10.f, 7.f});
    button.setFillColor(sf::Color(66, 92, 130));
    button.setOutlineColor(sf::Color(160, 190, 230));
    button.setOutlineThickness(1.f);
    window.draw(button);

    DrawText(window, fontPtr, "Generate (G)", 18.f, 12.f, 14, sf::Color::White);

    const std::string status = "Seed: " + std::to_string(currentSeed) +
                               "   Windows: " + std::to_string(desiredWindowCount) +
                               "   Save/Load: S/L   Grid: F3";
    DrawText(window, fontPtr, status, 150.f, 12.f, 14, sf::Color(220, 220, 220));

    if (showGrid) {
      DrawGrid(window, doc.gridWCells, doc.gridHCells);
    }

    DrawDocument(window, doc, fontPtr, hoveredId, edit.widgetId);

    window.display();
  }

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  CliOptions options;
  std::string error;
  if (!ParseArgs(argc, argv, &options, &error)) {
    if (!error.empty()) {
      std::cerr << "Argument error: " << error << "\n";
      return 1;
    }
    return 0;
  }

  try {
    return RunApp(options);
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return 1;
  }
}
