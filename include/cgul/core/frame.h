#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cgul {

struct Rgba8 { uint8_t r=255, g=255, b=255, a=255; };

enum CellFlags : uint32_t {
  None        = 0,
  Invert      = 1u << 0,
  Underline   = 1u << 1,
  Bold        = 1u << 2,
};

struct Cell {
  char32_t glyph = U' ';
  Rgba8 fg{};
  Rgba8 bg{0,0,0,255};
  uint32_t flags = CellFlags::None;
  uint32_t widgetId = 0; // 0 = none
};

struct Frame {
  int width = 0;
  int height = 0;
  std::vector<Cell> cells;

  Frame() = default;
  Frame(int w, int h);

  Cell& at(int x, int y);
  const Cell& at(int x, int y) const;

  void clear(char32_t glyph = U' ');
};

void draw_box(Frame& f, int x0, int y0, int x1, int y1, uint32_t widgetId);
void draw_text(Frame& f, int x, int y, const std::u32string& text, uint32_t widgetId);
uint32_t hit_test_widget(const Frame& f, int x, int y);

// “v0 JSON” dump: stable enough for inspection (no external dependency)
std::string to_json_v0(const Frame& f);

} // namespace cgul
