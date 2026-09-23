#include <SD.h>

#include "lib/image.h"

// Reads a big-endian 16-bit value from `p`.
static uint32_t readBe16(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 8) | p[1];
}

// Reads a big-endian 32-bit value from `p`.
static uint32_t readBe32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

// Reads a little-endian signed 32-bit value from `p`.
static int32_t readLe32(const uint8_t* p)
{
    return static_cast<int32_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                                (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24));
}

// Reads the pixel dimensions from the PNG IHDR chunk.
static bool getPngSize(File& file, int& width, int& height)
{
    static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    uint8_t header[24];
    if (file.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, kSignature, sizeof(kSignature)) != 0 || memcmp(header + 12, "IHDR", 4) != 0) {
        return false;
    }
    width = static_cast<int>(readBe32(header + 16));
    height = static_cast<int>(readBe32(header + 20));
    return true;
}

// Walks the JPEG marker segments until the first SOFn marker and reads the
// frame dimensions from it. The result is the stored frame size, before
// any EXIF orientation is applied.
static bool getJpgSize(File& file, int& width, int& height)
{
    uint8_t buf[4];
    if (file.read(buf, 2) != 2 || buf[0] != 0xFF || buf[1] != 0xD8) {
        return false;
    }
    while (true) {
        int c = file.read();
        if (c != 0xFF) {
            return false;
        }
        int marker;
        do {
            marker = file.read();
        } while (marker == 0xFF);
        if (marker < 0) {
            return false;
        }
        // Standalone markers without a length field.
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (file.read(buf, 2) != 2) {
            return false;
        }
        const uint32_t length = readBe16(buf);
        if (length < 2) {
            return false;
        }
        // SOF0-SOF15, excluding DHT (C4), JPG (C8), and DAC (CC).
        if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            uint8_t sof[5];
            if (file.read(sof, sizeof(sof)) != sizeof(sof)) {
                return false;
            }
            height = static_cast<int>(readBe16(sof + 1));
            width = static_cast<int>(readBe16(sof + 3));
            return true;
        }
        if (!file.seek(file.position() + length - 2)) {
            return false;
        }
    }
}

// Reads the pixel dimensions from the BMP info header. A negative height
// (top-down bitmap) is reported as its absolute value.
static bool getBmpSize(File& file, int& width, int& height)
{
    uint8_t header[26];
    if (file.read(header, sizeof(header)) != sizeof(header) || header[0] != 'B' || header[1] != 'M') {
        return false;
    }
    width = abs(readLe32(header + 18));
    height = abs(readLe32(header + 22));
    return true;
}

// Opens the SD file and dispatches to the header parser matching its
// extension. Succeeds when the parsed width and height are both positive.
bool getImageSize(const String& imagePath, int& width, int& height)
{
    String lower = imagePath;
    lower.toLowerCase();

    File file = SD.open(imagePath);
    if (!file) {
        return false;
    }

    bool ok = false;
    if (lower.endsWith(".png")) {
        ok = getPngSize(file, width, height);
    } else if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
        ok = getJpgSize(file, width, height);
    } else if (lower.endsWith(".bmp")) {
        ok = getBmpSize(file, width, height);
    }
    file.close();
    return ok && width > 0 && height > 0;
}

// Matches the extensions against a lowercased copy so that, e.g., ".JPG"
// is accepted too.
bool isSupportedImagePath(const String& path)
{
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".bmp");
}
