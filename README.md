# CHIP-8 Emulator

A CHIP-8 Emulator written in C++ with the help of SDL3.

Built mostly as a learning project to explore emulation, SDL and timing systems.

## Features

- CHIP-8 interpreter (as expected)
- SDL3 renderer
- keyboard input
- fixed timing system (CPU 500 Hz, Rendering 60 Hz)
- audio
- reloading rom
- pausing and muting audio support
- save and load state support

## Controls

| CHIP-8 | Keyboard |
|--------|----------|
| `1` `2` `3` `C` | `1` `2` `3` `4` |
| `4` `5` `6` `D` | `Q` `W` `E` `R` |
| `7` `8` `9` `E` | `A` `S` `D` `F` |
| `A` `0` `B` `F` | `Z` `X` `C` `V` |

Additionally:
- `F1` - Reloads ROM & CHIP-8
- `F5` - Save current state
- `F9` - Load past state
- `Space` - Pause emulator (Note: In games that render movement a lot for example pong during a "blink" this may cause some sprites to be not rendered)
- `M` - Disable/Re-enable audio

## Building

### Requirements

- C++20 compiler
- CMake
- SDL3

### Build

```bash
mkdir build
cd build
cmake ..
make
```

## How to run

```bash
./app <path-to-rom>
```

## Command Line Arguments

### Save Path

Overrides the default save-state location.

```bash
./app <path-to-rom> --save-path=<path>
```

### Quirks

```bash
./app <path-to-rom> --shift--uses-vy --jump-uses-vx --load-store-inc-i
```

**All of these are optional arguments**

## Screenshots

### Pong
![Pong](images/pong.png)

### IBM Test ROM
![IBM](images/ibm.png)

### Tetris
![Tetris](images/tetris.png)

## License

MIT License
