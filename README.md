# Spring Map Editor

A small map editor for Spring Engine SMF maps. It loads an `.smf` map together
with its `.smt` tile file, draws the terrain with an OpenGL 3.3 core profile
renderer, and lets you sculpt the heightmap, paint the metal map and paint colour
or texture onto the ground tiles. The whole editor interface is drawn in
OpenGL as a bar along the bottom of the window.

This repository was originally written around 2008 and has been revived to build
against a modern toolchain.

## Requirements

- CMake 3.16 or newer
- A C++17 compiler
- OpenGL 3.3 (core profile) and [GLEW](https://glew.sourceforge.net/)
- FreeType 2 (on screen text)
- [SDL3](https://www.libsdl.org/)
- [FreeImage](https://freeimage.sourceforge.io/) (optional, for the texture browser)

On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libsdl3-dev libglew-dev \
                 libfreetype-dev libfreeimage-dev
```

FTGL is no longer used: its fixed function glyph renderer does not work in a core
profile context, so text is drawn from a FreeType glyph atlas with a small
shader instead.

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
`-textures` preloads a directory of images for the texture brush and `-browser`
opens the thumbnail panel at start-up.

To verify that SDL, OpenGL and GLEW initialize correctly without loading a map:

```sh
./build/editor --check
```

The editor looks for a TrueType font in common system locations. To use a
specific font, set `SPRINGMAPEDITOR_FONT=/path/to/font.ttf`.

## The bottom bar

The bottom fifth of the window is the editor bar. Panels from left to right:

- **MAP**: `Open` shows a file picker for `.smf` maps, `Save` writes the map.
- **MODE**: `Hgt`, `Mtl`, `Tex`, `Col` choose what the brush edits.
- **BRUSH**: the brush shape (`O` circle, `[]` square, `/\` triangle), size and
  falloff (`-`/`+`, `F-`/`F+`).
- **VALUE**: the value the brush paints and `Pick` to take it from the map.
- **COLOUR**: red, green and blue sliders for fixed colour painting.
- **Right panel**: `Rect` switches to drag-rectangle fill, `Help` toggles the key
  list, `Map` cycles the metal/height map overlay, `Quit` exits and `Dir` opens
  the texture directory picker.
- **Thumbnails**: the images from the texture directory; click one to paint it.

The file and texture directory pickers list directories, `.smf` maps and image
files. Click a directory to enter it, use `<`, `>` and `Up` to page and to go to
the parent directory, and `Close` to dismiss the panel.

## Controls

The same list is printed on screen while the editor runs (toggle it with `Help`).

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
| `R` | Drag-rectangle fill |
| `L` | Metal / height map overlay |
| `W` | Wireframe |
| `F` | Fullscreen |
| `Ctrl+Z` | Undo one stroke |
| `Ctrl+Shift+Z` | Redo |
| `Esc` | Quit (asks whether to save) |

## Painting textures

Ground tiles are drawn with multitexturing: the tile texture is blended with a
paint layer that belongs to the big square it sits in. The paint layer has one
texel per map square and stores the painted colour in RGB and the coverage in A.
Strokes can be undone and redone per mouse stroke.

Give the editor a directory of images to paint with:

```sh
./build/editor /path/to/map.smf -textures /path/to/textures -browser
```

Click a thumbnail (or use `[` / `]`) to pick the texture. Without FreeImage only
`.bmp` files can be read.

Painting is baked into the compressed tiles only when the map is saved: the
affected tiles are decoded, blended with the paint layer and re-encoded as DXT1
with all four mip levels, deduplicated against the tiles already in the file and
appended to the last tile file. The tile map inside the `.smf` and the tile
counts in its header are rewritten to match. The minimap is not regenerated.

## Notes

- `Map/SM3/`, the bundled `ftgl/`, the fixed function lighting code and the
  MinGW libraries were removed as dead/unused code. Windows builds are intended
  to be done with MXE in the future.
- The map format code is little-endian and assumes the host is little-endian
  (`byteorder.h`).
- The renderer needs an OpenGL 3.3 core profile context; the shaders
  (`ground.vert/frag`, `flat.vert/frag`) are copied next to the binary at build
  time and loaded from the working directory.
