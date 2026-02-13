#include "cgul/core/frame.h"
#include "cgul/io/cgul_document.h"
#include "cgul/render/layout_composer.h"
#include "cgul/validate/validate.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct CliOptions {
  int hoverX = -1;
  int hoverY = -1;
  bool dumpJson = false;
  uint64_t seed = 0;
  std::string saveCgulPath;
  std::string loadCgulPath;
};

void PrintUsage(const char* exe) {
  std::cout
      << "Usage: " << exe << " [options]\n"
      << "  --save-cgul <path>  Save generated sample document\n"
      << "  --load-cgul <path>  Load, validate, compose and render a .cgul document\n"
      << "  --seed <u64>        Seed used by sample generator (default: 0)\n"
      << "  --hover <x> <y>     Print widget id under hovered cell\n"
      << "  --dump-json         Dump composed frame as v0 JSON\n";
}

bool ParseUInt64(const std::string& text, uint64_t* outValue) {
  if (outValue == nullptr || text.empty()) {
    return false;
  }
  uint64_t value = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    const uint64_t digit = static_cast<uint64_t>(ch - '0');
    if (value > (UINT64_MAX - digit) / 10ULL) {
      return false;
    }
    value = value * 10ULL + digit;
  }
  *outValue = value;
  return true;
}

bool ParseInt32(const std::string& text, int* outValue) {
  if (outValue == nullptr || text.empty()) {
    return false;
  }

  size_t index = 0;
  bool negative = false;
  if (text[index] == '-') {
    negative = true;
    ++index;
  }
  if (index >= text.size()) {
    return false;
  }

  int64_t value = 0;
  for (; index < text.size(); ++index) {
    const char ch = text[index];
    if (ch < '0' || ch > '9') {
      return false;
    }
    const int64_t digit = static_cast<int64_t>(ch - '0');
    if (value > (INT32_MAX - digit) / 10LL) {
      return false;
    }
    value = value * 10LL + digit;
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
      *outError = "internal argument parser error";
    }
    return false;
  }

  CliOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--hover") {
      if (i + 2 >= argc) {
        if (outError != nullptr) {
          *outError = "--hover requires two integer arguments";
        }
        return false;
      }
      if (!ParseInt32(argv[i + 1], &options.hoverX) || !ParseInt32(argv[i + 2], &options.hoverY)) {
        if (outError != nullptr) {
          *outError = "--hover arguments must be valid integers";
        }
        return false;
      }
      i += 2;
      continue;
    }

    if (arg == "--dump-json") {
      options.dumpJson = true;
      continue;
    }

    if (arg == "--save-cgul") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--save-cgul requires a path";
        }
        return false;
      }
      options.saveCgulPath = argv[++i];
      continue;
    }

    if (arg == "--load-cgul") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--load-cgul requires a path";
        }
        return false;
      }
      options.loadCgulPath = argv[++i];
      continue;
    }

    if (arg == "--seed") {
      if (i + 1 >= argc) {
        if (outError != nullptr) {
          *outError = "--seed requires an unsigned integer";
        }
        return false;
      }
      if (!ParseUInt64(argv[i + 1], &options.seed)) {
        if (outError != nullptr) {
          *outError = "--seed value must be a valid unsigned integer";
        }
        return false;
      }
      ++i;
      continue;
    }

    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
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
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

cgul::RectI GenerateRect(std::mt19937_64& rng, int gridW, int gridH, cgul::WidgetKind kind) {
  int minW = 8;
  int maxW = 24;
  int minH = 3;
  int maxH = 8;

  if (kind == cgul::WidgetKind::Window) {
    minW = 14;
    maxW = 26;
    minH = 5;
    maxH = 10;
  } else if (kind == cgul::WidgetKind::Label) {
    minW = 10;
    maxW = 24;
    minH = 3;
    maxH = 4;
  } else if (kind == cgul::WidgetKind::Button) {
    minW = 10;
    maxW = 18;
    minH = 3;
    maxH = 5;
  }

  maxW = std::min(maxW, gridW);
  maxH = std::min(maxH, gridH);
  minW = std::min(minW, maxW);
  minH = std::min(minH, maxH);

  std::uniform_int_distribution<int> widthDist(minW, maxW);
  std::uniform_int_distribution<int> heightDist(minH, maxH);

  const int w = widthDist(rng);
  const int h = heightDist(rng);

  std::uniform_int_distribution<int> xDist(0, gridW - w);
  std::uniform_int_distribution<int> yDist(0, gridH - h);

  return cgul::RectI{xDist(rng), yDist(rng), w, h};
}

cgul::CgulDocument GenerateSampleDocument(uint64_t seed) {
  cgul::CgulDocument doc;
  doc.cgulVersion = "0.1";
  doc.gridWCells = 60;
  doc.gridHCells = 20;
  doc.seed = seed;

  std::mt19937_64 rng(seed);
  std::uniform_int_distribution<int> countDist(3, 6);
  const int widgetCount = countDist(rng);

  const std::vector<cgul::WidgetKind> kinds = {
      cgul::WidgetKind::Window,
      cgul::WidgetKind::Panel,
      cgul::WidgetKind::Label,
      cgul::WidgetKind::Button,
  };
  std::uniform_int_distribution<size_t> kindDist(0, kinds.size() - 1);

  doc.widgets.clear();
  doc.widgets.reserve(static_cast<size_t>(widgetCount));

  uint32_t nextId = 1;
  for (int i = 0; i < widgetCount; ++i) {
    cgul::WidgetKind kind = (i == 0) ? cgul::WidgetKind::Window : kinds[kindDist(rng)];

    bool placed = false;
    cgul::RectI chosen;
    for (int tries = 0; tries < 256 && !placed; ++tries) {
      const cgul::RectI rect = GenerateRect(rng, doc.gridWCells, doc.gridHCells, kind);

      bool overlap = false;
      for (const cgul::Widget& existing : doc.widgets) {
        if (RectanglesOverlap(rect, existing.boundsCells)) {
          overlap = true;
          break;
        }
      }

      if (!overlap) {
        chosen = rect;
        placed = true;
      }
    }

    if (!placed) {
      for (int y = 0; y < doc.gridHCells && !placed; ++y) {
        for (int x = 0; x < doc.gridWCells && !placed; ++x) {
          cgul::RectI rect{x, y, 10, 4};
          if (rect.x + rect.w > doc.gridWCells || rect.y + rect.h > doc.gridHCells) {
            continue;
          }

          bool overlap = false;
          for (const cgul::Widget& existing : doc.widgets) {
            if (RectanglesOverlap(rect, existing.boundsCells)) {
              overlap = true;
              break;
            }
          }
          if (!overlap) {
            chosen = rect;
            placed = true;
          }
        }
      }
    }

    if (!placed) {
      continue;
    }

    cgul::Widget widget;
    widget.id = nextId++;
    widget.kind = kind;
    widget.boundsCells = chosen;

    switch (kind) {
      case cgul::WidgetKind::Window:
        widget.title = "Window " + std::to_string(widget.id);
        break;
      case cgul::WidgetKind::Panel:
        widget.title = "Panel " + std::to_string(widget.id);
        break;
      case cgul::WidgetKind::Label:
        widget.title = "Label " + std::to_string(widget.id);
        break;
      case cgul::WidgetKind::Button:
        widget.title = "Button " + std::to_string(widget.id);
        break;
    }

    doc.widgets.push_back(std::move(widget));
  }

  return doc;
}

void RenderTerminal(const cgul::Frame& frame, int hoverX, int hoverY) {
  for (int y = 0; y < frame.height; ++y) {
    for (int x = 0; x < frame.width; ++x) {
      const auto& c = frame.at(x, y);
      char ch = (c.glyph < 128) ? static_cast<char>(c.glyph) : '?';
      if (x == hoverX && y == hoverY) {
        ch = '@';
      }
      std::cout << ch;
    }
    std::cout << '\n';
  }
}

bool ValidateOrPrint(const cgul::CgulDocument& doc) {
  std::string error;
  if (!cgul::Validate(doc, &error)) {
    std::cerr << "Validation error: " << error << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  CliOptions options;
  std::string parseError;
  if (!ParseArgs(argc, argv, &options, &parseError)) {
    if (!parseError.empty()) {
      std::cerr << "Argument error: " << parseError << "\n";
    }
    PrintUsage(argv[0]);
    return parseError.empty() ? 0 : 1;
  }

  cgul::CgulDocument generatedDoc;
  bool generatedReady = false;

  auto ensureGenerated = [&]() -> const cgul::CgulDocument& {
    if (!generatedReady) {
      generatedDoc = GenerateSampleDocument(options.seed);
      generatedReady = true;
    }
    return generatedDoc;
  };

  if (!options.saveCgulPath.empty()) {
    const cgul::CgulDocument& doc = ensureGenerated();
    if (!ValidateOrPrint(doc)) {
      return 1;
    }

    std::string error;
    if (!cgul::SaveCgulFile(options.saveCgulPath, doc, &error)) {
      std::cerr << "Save error: " << error << "\n";
      return 1;
    }

    std::cout << "Saved .cgul: " << options.saveCgulPath << " (widgets=" << doc.widgets.size()
              << ", grid=" << doc.gridWCells << "x" << doc.gridHCells
              << ", seed=" << doc.seed << ")\n";
  }

  cgul::CgulDocument activeDoc;
  if (!options.loadCgulPath.empty()) {
    std::string error;
    if (!cgul::LoadCgulFile(options.loadCgulPath, &activeDoc, &error)) {
      std::cerr << "Load error: " << error << "\n";
      return 1;
    }
    if (!ValidateOrPrint(activeDoc)) {
      return 1;
    }

    std::cout << "Loaded .cgul: " << options.loadCgulPath << " (widgets=" << activeDoc.widgets.size()
              << ", grid=" << activeDoc.gridWCells << "x" << activeDoc.gridHCells
              << ", seed=" << activeDoc.seed << ")\n";
  } else {
    activeDoc = ensureGenerated();
    if (!ValidateOrPrint(activeDoc)) {
      return 1;
    }
  }

  cgul::Frame frame = cgul::ComposeLayoutToFrame(activeDoc);
  RenderTerminal(frame, options.hoverX, options.hoverY);

  if (options.hoverX >= 0 && options.hoverY >= 0) {
    const uint32_t widgetId = cgul::hit_test_widget(frame, options.hoverX, options.hoverY);
    std::cout << "\nHover cell (" << options.hoverX << "," << options.hoverY
              << ") widgetId=" << widgetId << "\n";
  }

  if (options.dumpJson) {
    std::cout << "\n" << cgul::to_json_v0(frame) << "\n";
  }

  return 0;
}
