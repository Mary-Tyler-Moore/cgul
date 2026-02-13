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
```

For non-interactive startup checks:

```bash
./build/cgul_demo_sfml --seed 123 --save /tmp/demo_layout.cgul --exit-after-startup
```

## Controls

- `G`: Generate a new deterministic layout (seed increments by 1)
- `S`: Save current layout (`demo_layout.cgul` by default, or `--save <path>`)
- `L`: Load current save path
- `F3`: Toggle cell grid
- `+` / `-`: Increase or decrease desired window count and regenerate
- `Esc`: Quit

Mouse editing:

- Left drag on title bar: move window (cell-snapped)
- Left drag on bottom-right handle: resize window (cell-snapped)

Editing rules:

- Minimum size: `10x6` cells
- Edits are clamped to grid bounds
- Any edit that fails CGUL validation (overlap/out-of-bounds/etc.) is rejected

## Font Loading

The demo tries fonts in this order:

1. `assets/fonts/DejaVuSansMono.ttf`
2. `/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf`

If neither path works, the demo still runs and prints a warning, but skips text draw.

## Save/Load Round-Trip

On save, the demo performs:

1. validate current document
2. save `.cgul`
3. reload saved file
4. validate reloaded document
5. semantic compare (`cgul::Equal`)

A PASS/FAIL message is printed to stdout/stderr.

This enforces a 1:1 semantic round-trip with the visible layout model.
