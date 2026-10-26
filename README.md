# Spring Map Editor

A small OpenGL editor for Spring Engine SMF maps. It loads an `.smf` map together
with its `.smt` tile file, renders the terrain, and lets you sculpt the heightmap
and paint the metal map.

This repository was originally written around 2008 and has been revived to build
against a modern toolchain.

## Requirements

- CMake 3.16 or newer
- A C++17 compiler
- OpenGL and GLU
- [GLEW](https://glew.sourceforge.net/)
- FreeType 2
- [SDL3](https://www.libsdl.org/)

On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libsdl3-dev libglew-dev \
                 libfreetype-dev libglu1-mesa-dev
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
./build/editor /path/to/map.smf [/path/to/map/directory]
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

| Input | Action |
| --- | --- |
| Left arrow / Right arrow | Rotate camera around the map |
| Up arrow / Down arrow | Tilt camera up / down |
| `Q` / `E` | Raise / lower camera |
| Left mouse drag | Orbit the camera |
| Right mouse drag | Smooth the current brush area |
| Mouse wheel | Raise / lower terrain (or metal, in metal mode) |
| `0`–`9` | Select editing mode (`0` = terrain, `1` = metal) |
| `A` / `Z` | Increase / decrease brush size |
| `C` / `X` | Increase / decrease brush height |
| `B` | Pick the height under the cursor |
| `L` | Show the heightmap/metalmap overlay |
| `S` | Toggle smoothing |
| `T` / `Y` | Decrease / increase terrain texture scale |
| `W` | Toggle wireframe |
| `F` | Toggle fullscreen |
| `Esc` | Quit (asks whether to save) |

## Notes

- `Map/SM3/` and the bundled MinGW libraries were removed as dead/unused code.
  Windows builds are intended to be done with MXE in the future.
- The map format code is little-endian and assumes the host is little-endian
  (`byteorder.h`).
