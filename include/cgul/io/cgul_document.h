#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cgul {

struct RectI {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

enum class WidgetKind {
  Window,
  Panel,
  Label,
  Button,
};

struct Widget {
  uint32_t id = 0;
  WidgetKind kind = WidgetKind::Panel;
  RectI boundsCells;
  std::string title;
};

struct CgulDocument {
  std::string cgulVersion = "0.1";
  int gridWCells = 0;
  int gridHCells = 0;
  uint64_t seed = 0;
  std::vector<Widget> widgets;
};

const char* ToString(WidgetKind kind);
bool ParseWidgetKind(const std::string& text, WidgetKind* outKind);

bool SaveCgulFile(const std::string& path, const CgulDocument& doc, std::string* outError);
bool LoadCgulFile(const std::string& path, CgulDocument* outDoc, std::string* outError);

}  // namespace cgul
