#pragma once

#include <Arduino.h>
#include <vector>

class M5PM1;

// Detects the SD card via the PM1 GPIO1 card-detect pin and initializes the
// SPI/SD bus. Returns false (and logs why) if no card is present or
// initialization fails. Call this only after `pm1` has been initialized.
bool sdBegin(M5PM1& pm1);

// Scans the SD card root and returns the paths ("/name") of the regular,
// visible files for which `filter` returns true. Hidden files (names
// starting with ".", such as macOS AppleDouble "._foo.png") are excluded
// before `filter` is called. Returns an empty list if the root cannot be
// opened.
std::vector<String> sdLoadFiles(bool (*filter)(const String& path));
