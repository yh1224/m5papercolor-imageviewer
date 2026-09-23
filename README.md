# m5papercolor-imageviewer

An image viewer firmware for M5PaperColor. It displays images stored in the root of an SD card on the color e-paper display.

## Features

- Supports PNG, JPEG, and BMP images (file extensions are case-insensitive).
- Landscape images are automatically rotated to fit the portrait screen.

## Usage

1. Copy image files to the root directory of an SD card.
2. Insert the SD card into the M5PaperColor and power it on.
3. Use the buttons to browse images:

| Button   | Action                                                                                  |
|----------|-----------------------------------------------------------------------------------------|
| Button A | Show the next image                                                                     |
| Button B | Show the previous image                                                                 |
| Button C | Reinitialize and return to the start screen (also reloads the SD card after swapping it) |

## Build & Flash

Requires [PlatformIO](https://platformio.org/).

```sh
# Build
pio run

# Build and upload to the device
pio run -t upload

# Open the serial monitor
pio device monitor
```

## Dependencies

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5PM1](https://github.com/m5stack/M5PM1)

See `platformio.ini` for the exact versions.
