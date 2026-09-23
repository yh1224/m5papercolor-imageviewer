#include <SD.h>
#include <SPI.h>
#include <M5PM1.h>
#include <vector>

#include "lib/sd.h"

static constexpr uint8_t SD_SPI_CS_PIN = 47;
static constexpr uint8_t SD_SPI_SCK_PIN = 15;
static constexpr uint8_t SD_SPI_MOSI_PIN = 13;
static constexpr uint8_t SD_SPI_MISO_PIN = 14;

// Checks the card-detect pin, then brings up the SPI bus and mounts the
// SD card on it.
bool sdBegin(M5PM1& pm1)
{
    // PM1 GPIO1 is the card-detect switch and reads LOW while a card is
    // inserted.
    if (pm1.digitalRead(M5PM1_GPIO_NUM_1) != LOW) {
        Serial.println("SD card not inserted.");
        return false;
    }

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("SD init failed.");
        return false;
    }

    return true;
}

// Prefixes `path` with "/" if it doesn't already start with one, matching
// the root-relative form the rest of this file expects.
static String normalizeRootPath(const char* path)
{
    String normalized = path == nullptr ? "" : String(path);
    if (!normalized.startsWith("/")) {
        normalized = "/" + normalized;
    }
    return normalized;
}

// Reports whether the file at `path` is hidden, i.e. its name starts
// with ".".
static bool isHiddenPath(const String& path)
{
    const int slash = path.lastIndexOf('/');
    const String base = slash >= 0 ? path.substring(slash + 1) : path;
    return base.startsWith(".");
}

// Walks the root directory once and keeps the matching files in the order
// the file system returns them.
std::vector<String> sdLoadFiles(bool (*filter)(const String& path))
{
    std::vector<String> paths;

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open SD root.");
        return paths;
    }

    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            // entry.name() may or may not include the leading "/"
            // depending on the core version, so normalize it first.
            const String path = normalizeRootPath(entry.name());
            if (!isHiddenPath(path) && filter(path)) {
                paths.push_back(path);
                Serial.printf("Found file: %s\n", path.c_str());
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();

    return paths;
}
