#include <Arduino.h>
#include <SD.h>
#include <M5GFX.h>
#include <M5PM1.h>
#include <M5Unified.h>
#include <vector>

#include "lib/image.h"
#include "lib/led.h"
#include "lib/sd.h"
#include "lib/step_queue.h"

// data/start.png is embedded by the linker via board_build.embed_files
// in platformio.ini; the symbol names derive from that path.
extern "C" const uint8_t _binary_data_start_png_start[];
extern "C" const uint8_t _binary_data_start_png_end[];
static const uint8_t* const kDefaultImagePng = _binary_data_start_png_start;
static const uint32_t kDefaultImagePngLen = static_cast<uint32_t>(
    _binary_data_start_png_end - _binary_data_start_png_start);

static M5PM1 pm1;

static std::vector<String> imagePaths;
static int currentImageIndex = -1;

// Image navigation is applied once the buttons have been idle for
// kNavigateDelayMs, accumulating repeated presses into a single redraw
// because each e-paper refresh is slow.
static const uint32_t kNavigateDelayMs = 1000;
static StepQueue stepQueue{kNavigateDelayMs};

static bool powerReady = false;
static bool sdReady = false;

static M5Canvas imageCanvas{&M5.Display};

// Brings up the PM1 power-management chip and the board power rails
// (EPD, SD card slot, and other GPIO-gated supplies) needed before the
// display, speaker, or SD card can be used. Returns false if PM1 itself
// fails to respond.
static bool beginPowerControl()
{
    const m5pm1_err_t err = pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    if (err != M5PM1_OK) {
        Serial.println("PM1 init failed.");
        return false;
    }

    pm1.setLdoEnable(true);
    // GPIO0 is the EPD power-enable line on this board (M5GFX's board
    // detection drives it high as a plain push-pull GPIO output), so it
    // must stay a standard GPIO output. Switching it to the PM1's
    // NeoPixel function cuts EPD power and hangs the display on refresh.
    pm1.pinMode(M5PM1_GPIO_NUM_0, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_0, HIGH);
    pm1.pinMode(M5PM1_GPIO_NUM_4, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_4, HIGH);
    pm1.pinMode(M5PM1_GPIO_NUM_3, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_3, HIGH);
    pm1.pinMode(M5PM1_GPIO_NUM_1, INPUT_PULLUP);
    return true;
}

// Draws the image embedded in the firmware, shown when no SD image is
// selected or the SD card is unavailable.
static bool drawDefaultImage(lgfx::LGFXBase& gfx, const int x, const int y, const int width, const int height)
{
    return gfx.drawPng(kDefaultImagePng, kDefaultImagePngLen, x, y, width, height, 0, 0, 0.0f, 0.0f, middle_center);
}

// Draws the SD image at `imagePath`, picking the decoder from its file
// extension. Returns false if the extension is unsupported or decoding
// fails.
static bool drawImage(lgfx::LGFXBase& gfx, const String& imagePath, const int x, const int y, const int width,
                      const int height)
{
    String lower = imagePath;
    lower.toLowerCase();

    if (lower.endsWith(".png")) {
        return gfx.drawPngFile(SD, imagePath.c_str(), x, y, width, height, 0, 0, 0.0f, 0.0f, middle_center);
    }
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
        return gfx.drawJpgFile(SD, imagePath.c_str(), x, y, width, height, 0, 0, 0.0f, 0.0f, middle_center);
    }
    if (lower.endsWith(".bmp")) {
        return gfx.drawBmpFile(SD, imagePath.c_str(), x, y, width, height, 0, 0, 0.0f, 0.0f, middle_center);
    }

    return false;
}

// Draws `text` at (x, y) with the given font, color, and alignment datum.
static void drawText(
    lgfx::LGFXBase& gfx, const String& text, const int x, const int y, const lgfx::IFont* font,
    const uint16_t color, const textdatum_t datum)
{
    gfx.setFont(font);
    gfx.setTextColor(color);
    gfx.setTextDatum(datum);
    gfx.drawString(text, x, y);
}

// Sets the on-board RGB LEDs to red while `loading` is true, or turns
// them off otherwise. Used to indicate that an image is being read and
// drawn.
static void setLedLoading(const bool loading)
{
    if (loading) {
        ledSetRgb(255, 0, 0);
    } else {
        ledSetRgb(0, 0, 0);
    }
}

// Redraws the screen with the default image or the currently selected SD
// image and its position in the list, lighting the loading LED and sounding
// a tone while the redraw is in progress.
static void updateImage()
{
    setLedLoading(true);
    M5.Speaker.tone(1000, 100);
    imageCanvas.fillSprite(WHITE);
    if (currentImageIndex < 0) {
        // draw default image
        drawDefaultImage(imageCanvas, 0, 0, M5.Display.width(), M5.Display.height() - 24);
        drawText(imageCanvas, String(imagePaths.size()) + " images",
                 M5.Display.width() - 8, M5.Display.height() - 8,
                 &FreeSans9pt7b, BLACK, bottom_right);
    } else {
        const String& imagePath = imagePaths[static_cast<size_t>(currentImageIndex)];
        // Landscape images are drawn rotated 90 degrees counterclockwise so
        // they fill the portrait screen; the canvas rotation is restored
        // before drawing the overlay text.
        int imageWidth = 0;
        int imageHeight = 0;
        const bool rotate = getImageSize(imagePath, imageWidth, imageHeight) && imageWidth > imageHeight;
        if (rotate) {
            imageCanvas.setRotation(3);
        }
        const bool drawn = drawImage(imageCanvas, imagePath, 0, 0, imageCanvas.width(), imageCanvas.height());
        imageCanvas.setRotation(0);
        if (!drawn) {
            drawText(imageCanvas, "Failed to draw image",
                     M5.Display.width() / 2, (M5.Display.height() - 24) / 2,
                     &FreeSansBold18pt7b, RED, middle_center);
        }
        drawText(imageCanvas, String(currentImageIndex + 1) + " / " + String(imagePaths.size()),
                 M5.Display.width() - 8, M5.Display.height() - 8,
                 &FreeSans9pt7b, BLACK, bottom_right);
    }
    imageCanvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
    M5.Speaker.tone(2000, 400);
    setLedLoading(false);
}

// Moves the current image selection by `step` positions, wrapping around
// the list, and redraws the screen. When no image is selected, a forward
// step of n selects the n-th image and a backward step of n the n-th
// image from the end.
static void selectImage(const int step)
{
    const size_t numImages = imagePaths.size();
    if (numImages == 0) {
        return;
    }

    const int n = static_cast<int>(numImages);
    int index;
    if (currentImageIndex < 0) {
        index = step > 0 ? step - 1 : n + step;
    } else {
        index = currentImageIndex + step;
    }
    currentImageIndex = ((index % n) + n) % n;
    Serial.printf("Selected image: %s\n", imagePaths[static_cast<size_t>(currentImageIndex)].c_str());
    updateImage();
}

// (Re)initializes power control, the SD card, the image list, and the
// speaker, discards any pending navigation, then shows the default image.
// Called from setup() and when BtnC is pressed.
void init()
{
    powerReady = beginPowerControl();
    if (powerReady) {
        sdReady = sdBegin(pm1);
    } else {
        Serial.println("Skipping SD init because PM1 is unavailable.");
        sdReady = false;
    }
    if (sdReady) {
        imagePaths = sdLoadFiles(isSupportedImagePath);
        if (imagePaths.empty()) {
            Serial.println("No supported image files found in SD root.");
        }
    } else {
        imagePaths.clear();
    }

    M5.Speaker.begin();
    M5.Speaker.setVolume(128);

    currentImageIndex = -1;
    stepQueue.clear();
    updateImage();
}

// Arduino entry point: waits for the USB serial port, initializes M5Unified
// and the display canvas, then runs init().
void setup()
{
    // This board uses USB CDC serial (ARDUINO_USB_CDC_ON_BOOT=1); right
    // after reset the port is still re-enumerating, so anything logged
    // during M5.begin() is lost unless we wait for a terminal to actually
    // attach first.
    Serial.begin(115200);
    const uint32_t serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 5000) {
        delay(10);
    }
    delay(500);
    Serial.println("Booting...");

    auto cfg = m5::M5Unified::config();
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Ex_I2C.begin();

    M5.Display.setRotation(0);
    M5.Display.setEpdMode(epd_quality);
    imageCanvas.createSprite(M5.Display.width(), M5.Display.height());

    init();
}

// Polls the buttons each cycle, queueing image navigation (drawn after the
// buttons go idle) or reinitializing back to the default image immediately.
// Navigation presses are accepted while there are images, each with a short
// click.
void loop()
{
    M5.update();
    const bool canNavigate = !imagePaths.empty();
    if (canNavigate && M5.BtnA.wasPressed()) {
        M5.Speaker.tone(1000, 30);
        stepQueue.push(1);
    }
    if (canNavigate && M5.BtnB.wasPressed()) {
        M5.Speaker.tone(1000, 30);
        stepQueue.push(-1);
    }
    if (M5.BtnC.wasPressed()) {
        init();
    }
    const int step = stepQueue.update();
    if (step != 0) {
        selectImage(step);
    }
    delay(50);
}
