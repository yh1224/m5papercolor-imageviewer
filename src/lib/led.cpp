#include "led.h"

#include <Arduino.h>
#include <esp32-hal-rmt.h>

// M5PaperColor's 2 on-board RGB LEDs are WS2812-style NeoPixels wired
// directly to ESP32 GPIO21. The PM1 chip's own NeoPixel pin, GPIO0, is
// reserved for the EPD's power-enable line on this board, so the LEDs
// are driven here directly with the ESP32 core's RMT peripheral, the
// same technique Arduino-ESP32's own neopixelWrite() helper uses.
static constexpr gpio_num_t RGB_LED_PIN = GPIO_NUM_21;
static constexpr size_t RGB_LED_COUNT = 2;

// Lazily initializes the RMT channel on first use, then sends the same
// color to every LED in the chain.
void ledSetRgb(const uint8_t r, const uint8_t g, const uint8_t b)
{
    static rmt_obj_t* rmtHandle = nullptr;
    static bool rmtReady = false;
    if (!rmtReady) {
        rmtHandle = rmtInit(RGB_LED_PIN, RMT_TX_MODE, RMT_MEM_64);
        if (rmtHandle == nullptr) {
            Serial.println("RGB LED RMT init failed.");
            return;
        }
        rmtSetTick(rmtHandle, 100); // 100 ns per tick
        rmtReady = true;
    }

    // WS2812 byte order is GRB, each byte MSB-first; ~0.4/0.8us high+low
    // per bit (T0H/T0L/T1H/T1L), matching Arduino-ESP32's neopixelWrite().
    const uint8_t colorBytes[3] = {g, r, b};
    rmt_data_t ledData[RGB_LED_COUNT * 24];
    size_t idx = 0;
    for (size_t led = 0; led < RGB_LED_COUNT; led++) {
        for (int byteIdx = 0; byteIdx < 3; byteIdx++) {
            for (int bit = 0; bit < 8; bit++) {
                // Fields are {duration0, level0, duration1, level1}.
                if (colorBytes[byteIdx] & (1 << (7 - bit))) {
                    ledData[idx] = {8, 1, 4, 0};
                } else {
                    ledData[idx] = {4, 1, 8, 0};
                }
                idx++;
            }
        }
    }
    rmtWriteBlocking(rmtHandle, ledData, RGB_LED_COUNT * 24);
}
