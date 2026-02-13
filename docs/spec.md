# CGUL Standard v0.1 (Canonical)

CGUL (Glyph UI Layout) defines UI semantics in a grid of monospace cells so the same meaning can be rendered by multiple backends.

This document is the canonical semantics reference for v0.1.

## 1. Coordinate and Frame Model

- A CGUL frame is a 2D grid of cells.
- Coordinates are integer cell coordinates, origin at top-left.
- `x` increases to the right.
- `y` increases downward.
- Bounds are represented as `{x, y, w, h}` in cell units.
- `w` and `h` are extents in cells and must be positive.

## 2. Widget Identity

- Each widget has a numeric `id`.
- `id = 0` is reserved as "no widget".
- Widget IDs must be unique within a document.

## 3. Widget Kinds (v0.1)

v0.1 defines a minimal semantic set:

- `window`
- `panel`
- `label`
- `button`

Backends may render these differently, but they should preserve semantic intent.

## 4. Document Semantics

A `CgulDocument` contains:

- `cgulVersion`: version string, must be `"0.1"` for this revision
- `gridWCells`, `gridHCells`: frame dimensions in cells
- `seed`: optional generation provenance (stored as integer in v0.1 files)
- `widgets`: ordered list of widgets

Widget bounds must remain inside the grid:

- `x >= 0`, `y >= 0`
- `x + w <= gridWCells`
- `y + h <= gridHCells`

## 5. Overlap Rule (v0.1)

For validation simplicity in v0.1:

- Window-to-window overlaps are forbidden.
- Other widget kind overlaps are not forbidden by the core validation rule.

This rule can evolve in later versions.

## 6. Reference Compositor Behavior

The v0.1 reference compositor maps a document to `cgul::Frame` by:

- Clearing the frame to spaces
- Drawing each widget bounds as an ASCII box
- Drawing title text near top-left of each widget
- Marking drawn cells with the widget `id`

For `window` widgets, the top row is styled as a simple title bar using ASCII `=`.

Adapters may render differently but should preserve IDs, bounds, and hit-test semantics.

## 7. Determinism Expectations

CGUL tooling should aim for deterministic behavior:

- Given the same seed and generator logic, produce the same widget layout.
- Writing `.cgul` should be deterministic in key ordering and formatting.
- Round-tripping load/save should preserve semantic content.
