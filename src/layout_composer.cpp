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

void DrawBoxBorder(Frame& frame, int x0, int y0, int x1, int y1, uint32_t widgetId) {
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      if (!InBounds(frame, x, y)) {
        continue;
      }

      Cell& c = frame.at(x, y);
      const bool left = (x == x0);
      const bool right = (x == x1);
      const bool top = (y == y0);
      const bool bottom = (y == y1);

      if (top) {
        if (left || right) {
          c.glyph = U'#';
        } else {
          c.glyph = U'=';
        }
      } else if (bottom || left || right) {
        c.glyph = U'#';
      } else {
        c.glyph = U' ';
      }

      c.widgetId = widgetId;
    }
  }
}

void DrawClippedText(Frame& frame, int x, int y, const std::string& text, int maxWidth,
                     uint32_t widgetId) {
  if (maxWidth <= 0 || text.empty() || y < 0 || y >= frame.height || x >= frame.width) {
    return;
  }

  const int startX = std::max(0, x);
  const int visibleWidth = std::min(maxWidth - (startX - x), frame.width - startX);
  if (visibleWidth <= 0) {
    return;
  }

  std::u32string glyphs = ToGlyphString(text);
  if (static_cast<int>(glyphs.size()) > visibleWidth) {
    glyphs.resize(static_cast<size_t>(visibleWidth));
  }

  draw_text(frame, startX, y, glyphs, widgetId);
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

    DrawBoxBorder(frame, x0, y0, x1, y1, widget.id);

    if (widget.kind == WidgetKind::Window) {
      const std::string title =
          widget.title.empty() ? ("Window " + std::to_string(widget.id)) : widget.title;
      DrawClippedText(frame, x0 + 2, y0, title, std::max(0, widget.boundsCells.w - 4), widget.id);

      const int interiorWidth = widget.boundsCells.w - 2;
      const int interiorHeight = widget.boundsCells.h - 2;
      if (interiorWidth > 0 && interiorHeight > 0) {
        const std::string line1 = "W x H: " + std::to_string(widget.boundsCells.w) + " x " +
                                  std::to_string(widget.boundsCells.h);
        DrawClippedText(frame, x0 + 1, y0 + 1, line1, interiorWidth, widget.id);

        if (interiorHeight > 1) {
          const std::string line2 = "pos: " + std::to_string(widget.boundsCells.x) + "," +
                                    std::to_string(widget.boundsCells.y);
          DrawClippedText(frame, x0 + 1, y0 + 2, line2, interiorWidth, widget.id);
        }
      }

      continue;
    }

    if (!widget.title.empty()) {
      const int textY = std::clamp(y0 + 1, y0, y1);
      DrawClippedText(frame, x0 + 1, textY, widget.title, std::max(0, widget.boundsCells.w - 2),
                      widget.id);
    }
  }

  return frame;
}

}  // namespace cgul
