# CGUL Integration Guide (v0.1)

## 1. Core Dependency Model

`cgul-core` is engine-agnostic C++20 and provides:

- frame primitives (`cgul::Frame`)
- document model (`cgul::CgulDocument`)
- IO (`LoadCgulFile`, `SaveCgulFile`)
- validation (`Validate`)
- reference composition (`ComposeLayoutToFrame`)

Do not introduce renderer or engine types into core modules.

## 2. Adapter Boundary

Render and host frameworks should live outside core:

- input adapter: framework event -> CGUL semantics
- render adapter: CGUL frame/document -> framework draw calls

Keep translation at the boundary so `cgul-core` remains portable.

## 3. Typical v0.1 Flow

1. Load document from `.cgul`
2. Validate document
3. Compose to `cgul::Frame`
4. Render frame in your environment
5. Optionally hit-test via `hit_test_widget`

## 4. Saving Documents

After user edits or generated changes:

1. Update `CgulDocument`
2. Validate
3. Save with `SaveCgulFile`

Writer output is deterministic for stable diffs and reproducible snapshots.
