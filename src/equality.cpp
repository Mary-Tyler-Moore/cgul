#include "cgul/core/equality.h"

#include <algorithm>
#include <vector>

namespace cgul {

namespace {

const std::string& NormalizeTitle(const Widget& widget) {
  return widget.title;
}

bool CompareWidgetId(const Widget& lhs, const Widget& rhs) {
  return lhs.id < rhs.id;
}

std::string RectToString(const RectI& rect) {
  return "x=" + std::to_string(rect.x) + ",y=" + std::to_string(rect.y) +
         ",w=" + std::to_string(rect.w) + ",h=" + std::to_string(rect.h);
}

}  // namespace

bool Equal(const RectI& a, const RectI& b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool Equal(const Widget& a, const Widget& b) {
  return a.id == b.id && a.kind == b.kind && Equal(a.boundsCells, b.boundsCells) &&
         NormalizeTitle(a) == NormalizeTitle(b);
}

bool Equal(const CgulDocument& a, const CgulDocument& b, std::string* outDiff) {
  if (outDiff != nullptr) {
    outDiff->clear();
  }

  auto fail = [outDiff](const std::string& msg) {
    if (outDiff != nullptr) {
      *outDiff = msg;
    }
    return false;
  };

  if (a.cgulVersion != b.cgulVersion) {
    return fail("cgulVersion mismatch: expected \"" + a.cgulVersion + "\" got \"" +
                b.cgulVersion + "\"");
  }
  if (a.gridWCells != b.gridWCells || a.gridHCells != b.gridHCells) {
    return fail("grid mismatch: expected " + std::to_string(a.gridWCells) + "x" +
                std::to_string(a.gridHCells) + " got " + std::to_string(b.gridWCells) + "x" +
                std::to_string(b.gridHCells));
  }
  if (a.seed != b.seed) {
    return fail("seed mismatch: expected " + std::to_string(a.seed) + " got " +
                std::to_string(b.seed));
  }
  if (a.widgets.size() != b.widgets.size()) {
    return fail("widget count mismatch: expected " + std::to_string(a.widgets.size()) + " got " +
                std::to_string(b.widgets.size()));
  }

  std::vector<Widget> aWidgets = a.widgets;
  std::vector<Widget> bWidgets = b.widgets;
  std::sort(aWidgets.begin(), aWidgets.end(), CompareWidgetId);
  std::sort(bWidgets.begin(), bWidgets.end(), CompareWidgetId);

  for (size_t i = 1; i < aWidgets.size(); ++i) {
    if (aWidgets[i - 1].id == aWidgets[i].id) {
      return fail("expected document has duplicate widget id " +
                  std::to_string(aWidgets[i].id));
    }
    if (bWidgets[i - 1].id == bWidgets[i].id) {
      return fail("got document has duplicate widget id " + std::to_string(bWidgets[i].id));
    }
  }

  for (size_t i = 0; i < aWidgets.size(); ++i) {
    const Widget& expected = aWidgets[i];
    const Widget& got = bWidgets[i];

    if (expected.id != got.id) {
      return fail("widget id set mismatch: expected id " + std::to_string(expected.id) +
                  " got id " + std::to_string(got.id));
    }

    if (expected.kind != got.kind) {
      return fail("Widget " + std::to_string(expected.id) + " kind mismatch: expected " +
                  ToString(expected.kind) + " got " + ToString(got.kind));
    }

    if (!Equal(expected.boundsCells, got.boundsCells)) {
      return fail("Widget " + std::to_string(expected.id) +
                  " bounds mismatch: expected " + RectToString(expected.boundsCells) + " got " +
                  RectToString(got.boundsCells));
    }

    const std::string expectedTitle = NormalizeTitle(expected);
    const std::string gotTitle = NormalizeTitle(got);
    if (expectedTitle != gotTitle) {
      return fail("Widget " + std::to_string(expected.id) + " title mismatch: expected \"" +
                  expectedTitle + "\" got \"" + gotTitle + "\"");
    }
  }

  return true;
}

}  // namespace cgul
