#include "glyph_grid_renderer.h"

#include <algorithm>

namespace {

std::string Utf8FromCodepoint(char32_t cp) {
  std::string out;
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
    return out;
  }

  if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    return out;
  }

  if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    return out;
  }

  if (cp <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    return out;
  }

  return {};
}

}  // namespace

bool GlyphGridRenderer::LoadFont(const std::vector<std::filesystem::path>& candidates,
                                 std::string* outLoadedPath) {
  mHasOwnedFont = false;
  mExternalFont = nullptr;

  for (const std::filesystem::path& path : candidates) {
    if (!std::filesystem::exists(path)) {
      continue;
    }

    if (mOwnedFont.openFromFile(path.string())) {
      mHasOwnedFont = true;
      if (outLoadedPath != nullptr) {
        *outLoadedPath = path.string();
      }
      return true;
    }
  }

  return false;
}

void GlyphGridRenderer::SetFont(const sf::Font* font) {
  mExternalFont = font;
}

void GlyphGridRenderer::SetHoveredCell(const std::optional<sf::Vector2i>& hoveredCell) {
  mHoveredCell = hoveredCell;
}

void GlyphGridRenderer::Draw(sf::RenderTarget& target, const cgul::Frame& frame,
                             const GlyphGridRenderConfig& config,
                             const sf::Vector2f& originPx) const {
  if (config.cellPx <= 0 || frame.width <= 0 || frame.height <= 0) {
    return;
  }

  const sf::Font* font = mExternalFont;
  if (font == nullptr && mHasOwnedFont) {
    font = &mOwnedFont;
  }

  sf::RectangleShape cellRect({static_cast<float>(config.cellPx), static_cast<float>(config.cellPx)});

  if (config.drawCellBackgrounds) {
    for (int y = 0; y < frame.height; ++y) {
      for (int x = 0; x < frame.width; ++x) {
        const cgul::Cell& cell = frame.at(x, y);
        if (cell.bg.a == 0) {
          continue;
        }

        cellRect.setPosition({originPx.x + static_cast<float>(x * config.cellPx),
                              originPx.y + static_cast<float>(y * config.cellPx)});
        cellRect.setFillColor(ToColor(cell.bg, sf::Color::Transparent));
        target.draw(cellRect);
      }
    }
  }

  if (font != nullptr && config.drawGridDots) {
    std::string dotString = Utf8FromCodepoint(config.dotGlyph);
    if (dotString.empty()) {
      dotString = ".";
    }

    sf::Text dot(*font, dotString, static_cast<unsigned int>(std::max(8, config.cellPx - 2)));
    dot.setFillColor(config.gridDotColor);

    for (int y = 0; y < frame.height; ++y) {
      for (int x = 0; x < frame.width; ++x) {
        const cgul::Cell& cell = frame.at(x, y);
        if (cell.glyph != U' ' || cell.widgetId != 0) {
          continue;
        }

        dot.setPosition({originPx.x + static_cast<float>(x * config.cellPx),
                         originPx.y + static_cast<float>(y * config.cellPx - 2)});
        target.draw(dot);
      }
    }
  }

  if (config.drawHoveredCell && mHoveredCell.has_value()) {
    const sf::Vector2i hover = *mHoveredCell;
    if (hover.x >= 0 && hover.y >= 0 && hover.x < frame.width && hover.y < frame.height) {
      cellRect.setPosition({originPx.x + static_cast<float>(hover.x * config.cellPx),
                            originPx.y + static_cast<float>(hover.y * config.cellPx)});
      cellRect.setFillColor(config.hoverCellColor);
      target.draw(cellRect);
    }
  }

  if (font != nullptr) {
    sf::Text glyph(*font, "", static_cast<unsigned int>(std::max(8, config.cellPx - 2)));
    for (int y = 0; y < frame.height; ++y) {
      for (int x = 0; x < frame.width; ++x) {
        const cgul::Cell& cell = frame.at(x, y);
        if (cell.glyph == U' ') {
          continue;
        }

        std::string glyphString = Utf8FromCodepoint(cell.glyph);
        if (glyphString.empty()) {
          glyphString = "?";
        }
        glyph.setString(glyphString);
        glyph.setPosition({originPx.x + static_cast<float>(x * config.cellPx),
                           originPx.y + static_cast<float>(y * config.cellPx - 2)});
        glyph.setFillColor(ToColor(cell.fg, config.defaultFg));
        target.draw(glyph);
      }
    }
    return;
  }

  // Font fallback: draw blocks for non-space glyphs and faint center marks for dotted grid.
  sf::RectangleShape block({static_cast<float>(std::max(2, config.cellPx / 2)),
                            static_cast<float>(std::max(2, config.cellPx / 2))});
  sf::RectangleShape dotBlock({1.f, 1.f});

  for (int y = 0; y < frame.height; ++y) {
    for (int x = 0; x < frame.width; ++x) {
      const cgul::Cell& cell = frame.at(x, y);
      const float baseX = originPx.x + static_cast<float>(x * config.cellPx);
      const float baseY = originPx.y + static_cast<float>(y * config.cellPx);

      if (config.drawGridDots && cell.glyph == U' ' && cell.widgetId == 0) {
        dotBlock.setPosition({baseX + static_cast<float>(config.cellPx / 2),
                              baseY + static_cast<float>(config.cellPx / 2)});
        dotBlock.setFillColor(config.gridDotColor);
        target.draw(dotBlock);
      }

      if (cell.glyph == U' ') {
        continue;
      }

      block.setPosition({baseX + static_cast<float>(config.cellPx / 4),
                         baseY + static_cast<float>(config.cellPx / 4)});
      block.setFillColor(ToColor(cell.fg, config.defaultFg));
      target.draw(block);
    }
  }
}

sf::Color GlyphGridRenderer::ToColor(const cgul::Rgba8& rgba, const sf::Color& fallback) {
  if (rgba.r == 0 && rgba.g == 0 && rgba.b == 0 && rgba.a == 0) {
    return fallback;
  }
  return sf::Color(rgba.r, rgba.g, rgba.b, rgba.a);
}
