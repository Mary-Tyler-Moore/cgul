#include "cgul/core/frame.h"
#include <iostream>
#include <string>

static void render_terminal(const cgul::Frame& f, int hx, int hy) {
  for (int y=0; y<f.height; ++y) {
    for (int x=0; x<f.width; ++x) {
      const auto& c = f.at(x,y);
      char ch = (c.glyph < 128) ? static_cast<char>(c.glyph) : '?';
      if (x==hx && y==hy) ch = '@';
      std::cout << ch;
    }
    std::cout << "\n";
  }
}

int main(int argc, char** argv) {
  int hoverX = -1, hoverY = -1;
  bool dumpJson = false;

  for (int i=1; i<argc; ++i) {
    std::string a = argv[i];
    if (a == "--hover" && i+2 < argc) {
      hoverX = std::stoi(argv[++i]);
      hoverY = std::stoi(argv[++i]);
    } else if (a == "--dump-json") {
      dumpJson = true;
    }
  }

  cgul::Frame f(60, 20);
  f.clear(U' ');

  // Sample UI
  cgul::draw_box(f, 1, 1, 58, 6, 1);
  cgul::draw_text(f, 3, 2, U"CGUL v0 (terminal renderer)", 1);

  cgul::draw_box(f, 3, 8, 20, 12, 100);
  cgul::draw_text(f, 7, 10, U"[ Button ]", 100);

  cgul::draw_box(f, 24, 8, 56, 12, 200);
  cgul::draw_text(f, 26, 10, U"Hover: wid shown below", 200);

  render_terminal(f, hoverX, hoverY);

  if (hoverX >= 0 && hoverY >= 0) {
    const auto wid = cgul::hit_test_widget(f, hoverX, hoverY);
    std::cout << "\nHover cell (" << hoverX << "," << hoverY << ") widgetId=" << wid << "\n";
  }

  if (dumpJson) {
    std::cout << "\n" << cgul::to_json_v0(f) << "\n";
  }

  return 0;
}
