#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Display Dimensions ========== */
#define ST7789V_WIDTH        240
#define ST7789V_HEIGHT       320

/* ========== Pin Definitions ========== */
#define ST7789V_PIN_SDA      48  // SPI MOSI
#define ST7789V_PIN_SCL      45  // SPI SCK
#define ST7789V_PIN_CS       47  // SPI Chip Select
#define ST7789V_PIN_RES      21  // Reset
#define ST7789V_PIN_DC       14  // Data/Command
#define ST7789V_PIN_BLK      -1  // Backlight (not used, -1 = disabled)

/* ========== SPI Configuration ========== */
#define ST7789V_SPI_HOST     SPI2_HOST
#define ST7789V_SPI_INIT_HZ  (10 * 1000 * 1000)  // 10 MHz for init
#define ST7789V_SPI_RUN_HZ   (40 * 1000 * 1000)  // 40 MHz for normal operation
#define ST7789V_SPI_QUEUE     10

/* ========== ST7789V Command Set ========== */
#define ST7789V_NOP          0x00
#define ST7789V_SWRESET      0x01
#define ST7789V_SLPIN        0x10
#define ST7789V_SLPOUT       0x11
#define ST7789V_PTLON        0x12
#define ST7789V_NORON        0x13
#define ST7789V_INVOFF       0x20
#define ST7789V_INVON        0x21
#define ST7789V_DISPOFF      0x28
#define ST7789V_DISPON       0x29
#define ST7789V_CASET        0x2A
#define ST7789V_RASET        0x2B
#define ST7789V_RAMWR        0x2C
#define ST7789V_MADCTL       0x36
#define ST7789V_COLMOD       0x3A

/* ========== RGB565 Color Macros ========== */
#define RGB565(r, g, b)  (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))
#define RGB565_R(c)      (((c) >> 8) & 0xF8)
#define RGB565_G(c)      (((c) >> 3) & 0xFC)
#define RGB565_B(c)      (((c) << 3) & 0xF8)

/* ========== Common Color Constants ========== */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFD20

#ifdef __cplusplus
}
#endif

/* ========== ST7789V Driver Class ========== */
class ST7789V {
public:
    ST7789V();

    /**
     * @brief Initialize the display (GPIO, SPI, reset, init sequence)
     * @return true on success
     */
    bool begin();

    /**
     * @brief Set display rotation (0=portrait, 1=landscape, 2=portrait inverted, 3=landscape inverted)
     * @param rot Rotation value 0-3
     */
    void setRotation(uint8_t rot);

    /**
     * @brief Fill entire screen with a single color
     * @param color RGB565 color
     */
    void fillScreen(uint16_t color);

    /**
     * @brief Draw a single pixel
     * @param x X coordinate
     * @param y Y coordinate
     * @param color RGB565 color
     */
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    /**
     * @brief Draw an unfilled rectangle
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param color RGB565 color
     */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /**
     * @brief Draw a filled rectangle (DMA optimized)
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param color RGB565 color
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    /**
     * @brief Set the drawing window (address window for pixel operations)
     * @param x0 Start X
     * @param y0 Start Y
     * @param x1 End X (inclusive)
     * @param y1 End Y (inclusive)
     */
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    /**
     * @brief Push an array of colors to the current address window (DMA transfer)
     * @param colors Pointer to RGB565 pixel data
     * @param len Number of pixels
     */
    void pushColors(uint16_t *colors, uint32_t len);

    /**
     * @brief Set backlight brightness (PWM)
     * @param brightness 0-255
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get current display width (accounts for rotation)
     * @return Width in pixels
     */
    int16_t width();

    /**
     * @brief Get current display height (accounts for rotation)
     * @return Height in pixels
     */
    int16_t height();

private:
    /**
     * @brief Send a command byte to the display
     * @param cmd Command byte
     */
    void writeCommand(uint8_t cmd);

    /**
     * @brief Send a data byte to the display
     * @param data Data byte
     */
    void writeData(uint8_t data);

    /**
     * @brief Send a 16-bit data word to the display
     * @param data 16-bit data
     */
    void writeData16(uint16_t data);

    /**
     * @brief Hardware reset the display
     */
    void reset();

    spi_device_handle_t _spi_dev;  ///< SPI device handle
    int16_t _width;                ///< Current width (after rotation)
    int16_t _height;               ///< Current height (after rotation)
    uint8_t _rotation;             ///< Current rotation setting
};
