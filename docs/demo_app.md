# CGUL Demo App v0 (SFML 3)

`cgul_demo_sfml` is a lightweight SFML 3 demo that renders and edits CGUL window layouts in cell units.

## Build

From repo root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCGUL_BUILD_SFML_DEMO=ON
cmake --build build
```

Notes:

- SFML 3.0.2 is pulled via CMake `FetchContent` when `CGUL_BUILD_SFML_DEMO=ON`.
- Core CGUL remains engine-agnostic; SFML usage is limited to this demo target.

## Run

```bash
./build/cgul_demo_sfml --seed 123
```

Optional startup IO:

```bash
./build/cgul_demo_sfml --seed 123 --save demo_layout.cgul
./build/cgul_demo_sfml --load demo_layout.cgul
./build/cgul_demo_sfml --load demo_layout.cgul --glyph
```

For non-interactive startup checks:

```bash
./build/cgul_demo_sfml --seed 123 --save /tmp/demo_layout.cgul --exit-after-startup
./build/cgul_demo_sfml --load /tmp/demo_layout.cgul --glyph --exit-after-startup
```

## Modes

The demo has two live rendering modes:

- **Pixel mode**: rectangle-based UI visualization
- **Glyph mode**: CGUL `Frame` visualization rendered as a glyph grid

Press `F1` to toggle modes at runtime without restarting.

Glyph mode is derived each frame from current semantic state via `ComposeLayoutToFrame(doc)`, so generation, drag/resize edits, and load operations appear immediately in the glyph grid.

Window text in glyph mode is rendered from the composed frame: title, `W x H`, and `pos` lines are clipped to window interior bounds so text never spills across borders.

## Controls

General:

- `F1`: Toggle Pixel/Glyph mode
- `G`: Generate a new deterministic layout (seed increments by 1)
- `S`: Save current layout (`demo_layout.cgul` by default, or `--save <path>`)
- `L`: Load current save path
- `F3`: Toggle grid overlay
  - Pixel mode: line grid
  - Glyph mode: dotted glyph background in empty cells
- `+` / `-`: Increase or decrease desired window count and regenerate
- `Esc`: Quit

Mouse editing (both modes):

- Left drag on title bar: move window (cell-snapped)
- Left drag on bottom-right handle: resize window (cell-snapped)

Editing rules:

- Minimum size: `18x6` cells (enforced for generation and resizing)
- Edits are clamped to grid bounds
- Any edit that fails CGUL validation (overlap/out-of-bounds/etc.) is rejected

Glyph mode HUD includes hover diagnostics:

- `Hover: (cx,cy) wid=<id>` where `wid` comes from `hit_test_widget(frame, cx, cy)`

## Font Loading

The demo uses a single repo-owned monospace font:

- `assets/fonts/cgul_mono.ttf`

This keeps rendering deterministic across machines. If this file is missing or fails to load, the demo still runs and prints a warning. Pixel text labels are skipped and glyph mode falls back to block markers for non-space cells.

## Save/Load Round-Trip

On save, the demo performs:

1. validate current document
2. save `.cgul`
3. reload saved file
4. validate reloaded document
5. semantic compare (`cgul::Equal`)

A PASS/FAIL message is printed to stdout/stderr.

This enforces a 1:1 semantic round-trip with the visible layout model.
