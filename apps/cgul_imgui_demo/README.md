# cgul_imgui_demo

Build and run from repo root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCGUL_BUILD_IMGUI_DEMO=ON
cmake --build build --target cgul_imgui_demo
./build/apps/cgul_imgui_demo/cgul_imgui_demo
```

Demo assets live under:

```text
apps/cgul_imgui_demo/assets/
```

Default export root:

```text
apps/cgul_imgui_demo/assets/chunks
```

CLI flags:

```text
--assets-dir <path>         Default: <app_dir>/assets
--default-browse-dir <path> Default: <assets-dir>
--output-root <path>        Default: <assets-dir>/chunks
```
