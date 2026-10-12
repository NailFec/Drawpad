# Drawpad

Drawpad is a drawing window for current Linux desktops. A touchpad is mapped onto the largest top-left region of the window that matches the touchpad's proportions. One finger draws. Two fingers that stay close pan the canvas. Once the fingers spread past a threshold, that gesture pans and zooms until both fingers lift.

The canvas has no fixed page size. Strokes are stored as cubic curves in double precision, so a written stroke keeps its shape when it is moved, saved, or viewed at a very large or very small scale.

## Requirements

- Linux with a current Wayland or X11 session. The window uses the normal platform window, so the same build is intended for niri, KDE Plasma, Hyprland, and other current desktops.
- Qt 6.12 with Qt Canvas Painter, Qt Gui, and Qt Widgets.
- libevdev and libudev.
- A Vulkan driver for the GPU that presents the window. If Vulkan cannot start, Drawpad uses OpenGL. Integrated Intel, AMD, and NVIDIA are all on this path.
- Membership in the `input` group, so the process can read the touchpad device.

Qt Canvas Painter 6.12 is available under the GNU General Public License version 3, or under a commercial Qt license. The Drawpad source is Apache License 2.0. A binary linked against the open-source Qt module has to comply with GPL-3.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/appDrawpad
```

If Qt Canvas Painter is not installed under the system prefix, CMake also searches `$HOME/.cache/drawpad/qt6-canvaspainter/usr`. Installing the distro package is preferred:

```sh
sudo pacman -S qt6-canvaspainter
```

`--vulkan` and `--opengl` select the renderer.

## Keys

- `P`, `E`, and `S` select the pen, eraser, and selection tool
- `1` through `6` select black, white, red, orange, blue, and green. `1` is the default
- `+` and `-` change the active pen or eraser width
- One finger in selection draws a lasso. Strokes that lie entirely inside it become selected. A later drag that starts inside that selection moves those strokes
- `Ctrl+S` saves, `Ctrl+Shift+S` saves to a new file, and `Ctrl+O` opens a file
- `Escape` quits and releases the touchpad

The touchpad is grabbed only while the window is active. A mouse can still move the window, resize it, and click the toolbar. Resizing keeps the same canvas region under the touchpad.

Documents are saved as `.drawpad` JSON. Opening a file replaces the current drawing.

## License

Apache License 2.0. Copyright [NailFec](https://nailfec.com) 2026.
