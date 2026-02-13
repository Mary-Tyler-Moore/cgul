# `.cgul` File Format v0.1

CGUL v0.1 uses JSON content with `.cgul` extension.

## 1. Encoding and Formatting

Writer requirements for deterministic output:

- UTF-8 text output
- 2-space indentation
- trailing newline at end of file
- stable key ordering (defined below)
- no trailing commas

## 2. Top-Level Object

Required keys in this order when writing:

1. `cgulVersion` (string)
2. `grid` (object)
3. `seed` (integer)
4. `widgets` (array)

`grid` object keys:

- `w` (int)
- `h` (int)

## 3. Widget Object

Each element in `widgets` is an object with keys in this order when writing:

1. `id` (int)
2. `kind` (string: `window|panel|label|button`)
3. `bounds` (object with `x,y,w,h` ints)
4. `title` (string, optional; omitted if absent)

## 4. Minimal Valid Example

```json
{
  "cgulVersion": "0.1",
  "grid": {
    "w": 20,
    "h": 8
  },
  "seed": 0,
  "widgets": [
    {
      "id": 1,
      "kind": "window",
      "bounds": {
        "x": 1,
        "y": 1,
        "w": 18,
        "h": 6
      },
      "title": "Main"
    }
  ]
}
```

## 5. Versioning

- Version is carried by `cgulVersion` string.
- v0.1 readers require `cgulVersion == "0.1"`.
- Later versions should not silently reinterpret v0.1 semantics.

## 6. Compatibility Guidance

- Parsers should ignore unknown keys when practical to ease forward compatibility.
- Required keys for v0.1 must still be present and type-correct.
- Floating-point numbers are not part of v0.1 numeric schema.
