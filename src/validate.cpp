#include "cgul/validate/validate.h"

#include <cstdint>
#include <string>
#include <unordered_set>

namespace cgul {

namespace {

bool RectsOverlap(const RectI& a, const RectI& b) {
  const int64_t aRight = static_cast<int64_t>(a.x) + static_cast<int64_t>(a.w);
  const int64_t bRight = static_cast<int64_t>(b.x) + static_cast<int64_t>(b.w);
  const int64_t aBottom = static_cast<int64_t>(a.y) + static_cast<int64_t>(a.h);
  const int64_t bBottom = static_cast<int64_t>(b.y) + static_cast<int64_t>(b.h);

  return a.x < bRight && b.x < aRight && a.y < bBottom && b.y < aBottom;
}

}  // namespace

bool Validate(const CgulDocument& doc, std::string* outError) {
  if (outError != nullptr) {
    outError->clear();
  }

  auto fail = [outError](const std::string& message) {
    if (outError != nullptr) {
      *outError = message;
    }
    return false;
  };

  if (doc.cgulVersion != "0.1") {
    return fail("cgulVersion must be \"0.1\"");
  }

  if (doc.gridWCells <= 0 || doc.gridHCells <= 0) {
    return fail("grid width and height must be > 0");
  }

  std::unordered_set<uint32_t> seenIds;
  seenIds.reserve(doc.widgets.size());

  for (size_t i = 0; i < doc.widgets.size(); ++i) {
    const Widget& widget = doc.widgets[i];

    if (widget.id == 0) {
      return fail("widget id must be non-zero (index " + std::to_string(i) + ")");
    }
    if (!seenIds.insert(widget.id).second) {
      return fail("duplicate widget id: " + std::to_string(widget.id));
    }

    if (widget.boundsCells.w <= 0 || widget.boundsCells.h <= 0) {
      return fail("widget " + std::to_string(widget.id) + " has non-positive bounds dimensions");
    }
    if (widget.boundsCells.x < 0 || widget.boundsCells.y < 0) {
      return fail("widget " + std::to_string(widget.id) + " has negative bounds origin");
    }

    const int64_t right = static_cast<int64_t>(widget.boundsCells.x) +
                          static_cast<int64_t>(widget.boundsCells.w);
    const int64_t bottom = static_cast<int64_t>(widget.boundsCells.y) +
                           static_cast<int64_t>(widget.boundsCells.h);

    if (right > doc.gridWCells || bottom > doc.gridHCells) {
      return fail("widget " + std::to_string(widget.id) + " bounds exceed grid limits");
    }
  }

  for (size_t i = 0; i < doc.widgets.size(); ++i) {
    const Widget& a = doc.widgets[i];
    if (a.kind != WidgetKind::Window) {
      continue;
    }
    for (size_t j = i + 1; j < doc.widgets.size(); ++j) {
      const Widget& b = doc.widgets[j];
      if (b.kind != WidgetKind::Window) {
        continue;
      }
      if (RectsOverlap(a.boundsCells, b.boundsCells)) {
        return fail("window overlap is not allowed in v0.1 (ids " +
                    std::to_string(a.id) + " and " + std::to_string(b.id) + ")");
      }
    }
  }

  return true;
}

}  // namespace cgul
