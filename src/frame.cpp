#include "cgul/core/frame.h"
#include <sstream>

namespace cgul {

Frame::Frame(int w, int h) : width(w), height(h), cells(static_cast<size_t>(w*h)) {}

Cell& Frame::at(int x, int y) {
  return cells[static_cast<size_t>(y*width + x)];
}
const Cell& Frame::at(int x, int y) const {
  return cells[static_cast<size_t>(y*width + x)];
}

void Frame::clear(char32_t glyph) {
  for (auto& c : cells) {
    c.glyph = glyph;
    c.fg = {};
    c.bg = {0,0,0,255};
    c.flags = CellFlags::None;
    c.widgetId = 0;
  }
}

static bool in_bounds(const Frame& f, int x, int y) {
  return x >= 0 && y >= 0 && x < f.width && y < f.height;
}

void draw_box(Frame& f, int x0, int y0, int x1, int y1, uint32_t widgetId) {
  // inclusive box
  for (int y=y0; y<=y1; ++y) {
    for (int x=x0; x<=x1; ++x) {
      if (!in_bounds(f,x,y)) continue;
      auto& c = f.at(x,y);
      const bool edge = (x==x0 || x==x1 || y==y0 || y==y1);
      c.glyph = edge ? U'#' : U' ';
      c.widgetId = widgetId;
    }
  }
}

void draw_text(Frame& f, int x, int y, const std::u32string& text, uint32_t widgetId) {
  for (size_t i=0; i<text.size(); ++i) {
    const int xx = x + static_cast<int>(i);
    if (!in_bounds(f,xx,y)) continue;
    auto& c = f.at(xx,y);
    c.glyph = text[i];
    c.widgetId = widgetId;
  }
}

uint32_t hit_test_widget(const Frame& f, int x, int y) {
  if (!in_bounds(f,x,y)) return 0;
  return f.at(x,y).widgetId;
}

static void json_escape(std::ostringstream& os, const std::string& s) {
  for (char ch : s) {
    switch (ch) {
      case '\"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default: os << ch; break;
    }
  }
}

static std::string to_utf8(char32_t c) {
  // Minimal: only ASCII for v0
  if (c < 0x80) return std::string(1, static_cast<char>(c));
  return "?";
}

std::string to_json_v0(const Frame& f) {
  std::ostringstream os;
  os << "{";
  os << "\"w\":" << f.width << ",\"h\":" << f.height << ",\"cells\":[";
  for (int y=0; y<f.height; ++y) {
    if (y) os << ",";
    os << "[";
    for (int x=0; x<f.width; ++x) {
      if (x) os << ",";
      const auto& c = f.at(x,y);
      os << "{";
      os << "\"g\":\""; json_escape(os, to_utf8(c.glyph)); os << "\",";
      os << "\"wid\":" << c.widgetId;
      os << "}";
    }
    os << "]";
  }
  os << "]}";
  return os.str();
}

} // namespace cgul
