#pragma once

#include <Arduino.h>

// Reads the pixel dimensions of the SD image at `imagePath` from its file
// header, picking the parser from its file extension. Returns false if the
// extension is unsupported or the header cannot be parsed.
bool getImageSize(const String& imagePath, int& width, int& height);

// Reports whether `path` has an image extension this firmware can decode
// and display (.png/.jpg/.jpeg/.bmp, case-insensitive).
bool isSupportedImagePath(const String& path);
