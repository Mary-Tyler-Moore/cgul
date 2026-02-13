#pragma once

#include "cgul/core/frame.h"

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct GlyphGridRenderConfig {
  int cellPx = 16;
  bool drawCellBackgrounds = false;
  bool drawGridDots = false;
  char32_t dotGlyph = U'·';
  bool drawHoveredCell = true;
  sf::Color defaultFg = sf::Color(220, 220, 220);
  sf::Color gridDotColor = sf::Color(120, 120, 130, 110);
  sf::Color hoverCellColor = sf::Color(120, 170, 255, 80);
};

class GlyphGridRenderer {
 public:
  bool LoadFont(const std::vector<std::filesystem::path>& candidates, std::string* outLoadedPath);
  void SetFont(const sf::Font* font);
  void SetHoveredCell(const std::optional<sf::Vector2i>& hoveredCell);

  void Draw(sf::RenderTarget& target, const cgul::Frame& frame, const GlyphGridRenderConfig& config,
            const sf::Vector2f& originPx) const;

 private:
  static sf::Color ToColor(const cgul::Rgba8& rgba, const sf::Color& fallback);

  sf::Font mOwnedFont;
  const sf::Font* mExternalFont = nullptr;
  bool mHasOwnedFont = false;
  std::optional<sf::Vector2i> mHoveredCell;
};
