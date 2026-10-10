#pragma once

namespace board_e32r40t {
static constexpr int PIN_TFT_MISO = 12;
static constexpr int PIN_TFT_MOSI = 13;
static constexpr int PIN_TFT_SCLK = 14;
static constexpr int PIN_TFT_CS   = 15;
static constexpr int PIN_TFT_DC   = 2;
static constexpr int PIN_TFT_BL   = 27;
static constexpr int PIN_TOUCH_CS = 33;
static constexpr int PIN_TOUCH_IRQ = 36;

static constexpr int SD_CS   = 5;
static constexpr int SD_MOSI = 23;
static constexpr int SD_SCLK = 18;
static constexpr int SD_MISO = 19;

static constexpr int BAT_ADC = 34;
static constexpr float BAT_DIVIDER = 2.0f;

static constexpr int LED_R = 22;
static constexpr int LED_G = 16;
static constexpr int LED_B = 17;
static constexpr int AUDIO_ENABLE = 4;
static constexpr int AUDIO_DAC = 26;
}