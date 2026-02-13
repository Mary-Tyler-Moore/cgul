#include "cgul/render/layout_composer.h"

#include <algorithm>
#include <string>

namespace cgul {

namespace {

std::u32string ToGlyphString(const std::string& text) {
  std::u32string result;
  result.reserve(text.size());
  for (unsigned char ch : text) {
    result.push_back(ch < 128 ? static_cast<char32_t>(ch) : U'?');
  }
  return result;
}

bool InBounds(const Frame& frame, int x, int y) {
  return x >= 0 && y >= 0 && x < frame.width && y < frame.height;
}

void DrawWindowTitleBar(Frame& frame, int x0, int x1, int y, uint32_t widgetId) {
  if (y < 0 || y >= frame.height) {
    return;
  }
  for (int x = x0 + 1; x <= x1 - 1; ++x) {
    if (!InBounds(frame, x, y)) {
      continue;
    }
    Cell& c = frame.at(x, y);
    c.glyph = U'=';
    c.widgetId = widgetId;
  }
}

}  // namespace

Frame ComposeLayoutToFrame(const CgulDocument& doc) {
  Frame frame(doc.gridWCells, doc.gridHCells);
  frame.clear(U' ');

  for (const Widget& widget : doc.widgets) {
    const int x0 = widget.boundsCells.x;
    const int y0 = widget.boundsCells.y;
    const int x1 = widget.boundsCells.x + widget.boundsCells.w - 1;
    const int y1 = widget.boundsCells.y + widget.boundsCells.h - 1;

    draw_box(frame, x0, y0, x1, y1, widget.id);

    if (widget.kind == WidgetKind::Window) {
      DrawWindowTitleBar(frame, x0, x1, y0, widget.id);
    }

    if (!widget.title.empty()) {
      int textY = (widget.kind == WidgetKind::Window) ? y0 : y0 + 1;
      textY = std::clamp(textY, y0, y1);

      int textX = (widget.kind == WidgetKind::Window) ? x0 + 2 : x0 + 1;
      textX = std::clamp(textX, x0, x1);

      const int available = x1 - textX + 1;
      if (available > 0) {
        std::u32string title = ToGlyphString(widget.title);
        if (static_cast<int>(title.size()) > available) {
          title.resize(static_cast<size_t>(available));
        }
        draw_text(frame, textX, textY, title, widget.id);
      }
    }
  }

  return frame;
}

}  // namespace cgul
