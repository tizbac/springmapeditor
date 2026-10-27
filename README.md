# Spring Map Editor

A small OpenGL editor for Spring Engine SMF maps. It loads an `.smf` map together
with its `.smt` tile file, renders the terrain, and lets you sculpt the heightmap,
paint the metal map and paint colour or texture onto the ground tiles.

This repository was originally written around 2008 and has been revived to build
against a modern toolchain.

## Requirements

- CMake 3.16 or newer
- A C++17 compiler
- OpenGL and GLU
- [GLEW](https://glew.sourceforge.net/)
- FreeType 2
- [SDL3](https://www.libsdl.org/)
- [FreeImage](https://freeimage.sourceforge.io/) (optional, for the texture browser)

On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libsdl3-dev libglew-dev \
                 libfreetype-dev libglu1-mesa-dev libfreeimage-dev
```

FTGL is vendored in `ftgl/` and built automatically, so no system FTGL package is
needed.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```sh
./build/editor /path/to/map.smf [/path/to/map/directory] [-textures <dir>] [-browser]
```

The map directory is where the `.smt` tile file and optional detail textures are
found. If it is omitted, the directory containing the `.smf` file is used.

To verify that SDL, OpenGL and GLEW initialize correctly without loading a map:

```sh
./build/editor --check
```

The editor looks for a TrueType font in common system locations. To use a
specific font, set `SPRINGMAPEDITOR_FONT=/path/to/font.ttf`.

## Controls

The same list is printed on screen while the editor runs.

| Input | Action |
| --- | --- |
| Left mouse drag | Apply the brush |
| Right mouse drag | Orbit the camera around the point under the cursor |
| Mouse wheel, `Q` / `E` | Zoom in / out |
| Arrow keys | Pan the camera |
| `A` / `Z` | Larger / smaller brush |
| `C` / `X` | Larger / smaller brush value |
| `B` | Pick the ground value under the cursor as the brush value |
| `S` | Smooth the ground instead of painting it |
| `P` | Cycle the brush shape (circle, square, triangle) |
| `-` / `=` | Less / more brush falloff |
| `0` – `3` | Mode: height, metal, texture, colour |
| `T` / `Y` | Texture detail (which mip level is drawn) |
| `[` / `]` | Previous / next texture |
| `,` / `.` | Texture paint scale |
| `G` | Toggle the texture browser |
| `W` | Wireframe |
| `F` | Fullscreen |
| `Ctrl+Z` | Undo one stroke |
| `Ctrl+Shift+Z` | Redo |
| `Esc` | Quit (asks whether to save) |

## Painting textures

Ground tiles are drawn with multitexturing: the tile texture is blended with a
paint layer that belongs to the big square it sits in. The paint layer has one
texel per map square and stores the painted colour in RGB and the coverage in A.

Give the editor a directory of images to paint with:

```sh
./build/editor /path/to/map.smf -textures /path/to/textures -browser
```

`-browser` opens the thumbnail panel; click a thumbnail (or use `[` / `]`) to
pick the texture. Without FreeImage only `.bmp` files can be read.

Painting is baked into the compressed tiles only when the map is saved: the
affected tiles are decoded, blended with the paint layer and re-encoded as DXT1
with all four mip levels, deduplicated against the tiles already in the file and
appended to the last tile file. The tile map inside the `.smf` and the tile
counts in its header are rewritten to match. The minimap is not regenerated.

## Notes

- `Map/SM3/` and the bundled MinGW libraries were removed as dead/unused code.
  Windows builds are intended to be done with MXE in the future.
- The map format code is little-endian and assumes the host is little-endian
  (`byteorder.h`).
