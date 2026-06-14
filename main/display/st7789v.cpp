#include "st7789v.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ST7789V";

/* ========== Constructor ========== */
ST7789V::ST7789V()
    : _spi_dev(nullptr)
    , _width(ST7789V_WIDTH)
    , _height(ST7789V_HEIGHT)
    , _rotation(0)
{
}

/* ========== begin() ========== */
bool ST7789V::begin()
{
    esp_err_t ret;

    /* --- GPIO Init --- */
    /* 注意：CS 由 SPI 硬件控制，不需要手动配置 */
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << ST7789V_PIN_DC) | (1ULL << ST7789V_PIN_RES);
    /* ST7789V_PIN_BLK is -1 (disabled), skip it */
    /* ST7789V_PIN_CS 由 SPI 硬件控制 */
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* --- SPI Bus Init (10 MHz for initialization) --- */
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = ST7789V_PIN_SDA;
    bus_cfg.miso_io_num = -1;  // No MISO needed
    bus_cfg.sclk_io_num = ST7789V_PIN_SCL;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = ST7789V_WIDTH * ST7789V_HEIGHT * 2 + 8;

    ret = spi_bus_initialize(ST7789V_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* --- Attach Device to SPI Bus --- */
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = ST7789V_SPI_INIT_HZ;
    dev_cfg.mode = 3;               // SPI mode 3 (CPOL=1, CPHA=1) - 尝试不同的模式
    dev_cfg.spics_io_num = ST7789V_PIN_CS;
    dev_cfg.queue_size = ST7789V_SPI_QUEUE;
    dev_cfg.pre_cb = nullptr;       // No pre-transfer callback (DC handled manually)
    dev_cfg.post_cb = nullptr;

    ret = spi_bus_add_device(ST7789V_SPI_HOST, &dev_cfg, &_spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* --- Hardware Reset --- */
    reset();

    /* --- Software Reset --- */
    writeCommand(ST7789V_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    /* --- Sleep Out --- */
    writeCommand(ST7789V_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* --- Color Mode: 16-bit RGB565 (0x55) --- */
    writeCommand(ST7789V_COLMOD);
    writeData(0x55);  // 16-bit color mode (RGB565)
    vTaskDelay(pdMS_TO_TICKS(10));

    /* --- Memory Data Access Control --- */
    writeCommand(ST7789V_MADCTL);
    writeData(0x00);

    /* --- Porch Setting --- */
    writeCommand(0xB2);  // Porch Control
    writeData(0x0C);
    writeData(0x0C);
    writeData(0x00);
    writeData(0x33);
    writeData(0x33);

    /* --- Gate Control --- */
    writeCommand(0xB7);
    writeData(0x75);  // VGH=15V, VGL=-10.43

    /* --- VCOM Setting --- */
    writeCommand(0xBB);
    writeData(0x21);  // Vcom

    /* --- LCM Control --- */
    writeCommand(0xC0);
    writeData(0x2C);

    /* --- VDV and VRH Command Enable --- */
    writeCommand(0xC2);
    writeData(0x01);

    /* --- VRH Set --- */
    writeCommand(0xC3);
    writeData(0x0B);  // GVDD=4.55v

    /* --- VDV Set --- */
    writeCommand(0xC4);
    writeData(0x20);  // VDV, 0x20:0v

    /* --- Frame Rate Control --- */
    writeCommand(0xC6);
    writeData(0x0F);  // 60Hz

    /* --- Power Control --- */
    writeCommand(0xD0);
    writeData(0xA4);
    writeData(0xA1);

    /* --- D6 Setting --- */
    writeCommand(0xD6);
    writeData(0xA1);

    /* --- Positive Voltage Gamma Control --- */
    writeCommand(0xE0);
    writeData(0xD0);
    writeData(0x06);
    writeData(0x0B);
    writeData(0x09);
    writeData(0x08);
    writeData(0x30);
    writeData(0x30);
    writeData(0x5B);
    writeData(0x4B);
    writeData(0x18);
    writeData(0x14);
    writeData(0x14);
    writeData(0x2C);
    writeData(0x32);

    /* --- Negative Voltage Gamma Control --- */
    writeCommand(0xE1);
    writeData(0xD0);
    writeData(0x05);
    writeData(0x0A);
    writeData(0x0A);
    writeData(0x07);
    writeData(0x28);
    writeData(0x32);
    writeData(0x2C);
    writeData(0x49);
    writeData(0x18);
    writeData(0x13);
    writeData(0x13);
    writeData(0x2C);
    writeData(0x33);

    /* --- Display Inversion On --- */
    writeCommand(0x21);

    /* --- Display On --- */
    writeCommand(ST7789V_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Increase SPI Speed to 40 MHz --- */
    dev_cfg.clock_speed_hz = ST7789V_SPI_RUN_HZ;
    ret = spi_bus_remove_device(_spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device remove failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = spi_bus_add_device(ST7789V_SPI_HOST, &dev_cfg, &_spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device re-add failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* --- Clear Screen --- */
    fillScreen(COLOR_BLACK);

    ESP_LOGI(TAG, "ST7789V initialized (%dx%d @ %d MHz)", _width, _height, ST7789V_SPI_RUN_HZ / 1000000);
    return true;
}

/* ========== setRotation() ========== */
void ST7789V::setRotation(uint8_t rot)
{
    _rotation = rot % 4;
    uint8_t madctl = 0x00;

    switch (_rotation) {
        case 0: // Portrait
            madctl = 0x00;
            _width  = ST7789V_WIDTH;
            _height = ST7789V_HEIGHT;
            break;
        case 1: // Landscape
            madctl = 0x60; // MY=0, MX=1, MV=1, ML=0
            _width  = ST7789V_HEIGHT;
            _height = ST7789V_WIDTH;
            break;
        case 2: // Portrait Inverted
            madctl = 0xC0; // MY=1, MX=0, MV=0, ML=0
            _width  = ST7789V_WIDTH;
            _height = ST7789V_HEIGHT;
            break;
        case 3: // Landscape Inverted
            madctl = 0xA0; // MY=1, MX=0, MV=1, ML=0
            _width  = ST7789V_HEIGHT;
            _height = ST7789V_WIDTH;
            break;
    }

    writeCommand(ST7789V_MADCTL);
    writeData(madctl);
}

/* ========== fillScreen() ========== */
void ST7789V::fillScreen(uint16_t color)
{
    fillRect(0, 0, _width, _height, color);
}

/* ========== drawPixel() ========== */
void ST7789V::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;

    setAddrWindow(x, y, x, y);
    /* 16-bit RGB565 模式 */
    writeData16(color);
}

/* ========== drawRect() ========== */
void ST7789V::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    // Top line
    fillRect(x, y, w, 1, color);
    // Bottom line
    fillRect(x, y + h - 1, w, 1, color);
    // Left line
    fillRect(x, y, 1, h, color);
    // Right line
    fillRect(x + w - 1, y, 1, h, color);
}

/* ========== fillRect() (16-bit RGB565 mode) ========== */
void ST7789V::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (x >= _width || y >= _height || w <= 0 || h <= 0) return;

    // Clip to screen bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width)  w = _width - x;
    if (y + h > _height) h = _height - y;

    setAddrWindow(x, y, x + w - 1, y + h - 1);

    // Allocate a line buffer in DMA-capable memory (16-bit per pixel)
    uint16_t *line_buf = (uint16_t *)heap_caps_malloc(w * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line_buf) {
        ESP_LOGE(TAG, "Failed to allocate line buffer for fillRect");
        return;
    }

    // Fill the line buffer with the color
    for (int16_t i = 0; i < w; i++) {
        line_buf[i] = color;
    }

    // Push the same line buffer for each row
    gpio_set_level((gpio_num_t)ST7789V_PIN_DC, 1);  // DC high = data

    spi_transaction_t t = {};
    t.length = w * 16;  // 16 bits per pixel
    t.tx_buffer = line_buf;

    for (int16_t row = 0; row < h; row++) {
        spi_device_transmit(_spi_dev, &t);
    }

    heap_caps_free(line_buf);
}

/* ========== setAddrWindow() ========== */
void ST7789V::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Column Address Set
    writeCommand(ST7789V_CASET);
    writeData16(x0);
    writeData16(x1);

    // Row Address Set
    writeCommand(ST7789V_RASET);
    writeData16(y0);
    writeData16(y1);

    // Memory Write
    writeCommand(ST7789V_RAMWR);
}

/* ========== pushColors() (DMA transfer) ========== */
void ST7789V::pushColors(uint16_t *colors, uint32_t len)
{
    if (!colors || len == 0) return;

    gpio_set_level((gpio_num_t)ST7789V_PIN_DC, 1);  // DC high = data

    spi_transaction_t t = {};
    t.length = len * 16;  // 16 bits per pixel
    t.tx_buffer = colors;
    spi_device_transmit(_spi_dev, &t);
}

/* ========== setBrightness() ========== */
void ST7789V::setBrightness(uint8_t brightness)
{
    if (ST7789V_PIN_BLK < 0) return;
    // Simple on/off since no PWM configured here
    gpio_set_level((gpio_num_t)ST7789V_PIN_BLK, brightness > 0 ? 1 : 0);
}

/* ========== width() ========== */
int16_t ST7789V::width()
{
    return _width;
}

/* ========== height() ========== */
int16_t ST7789V::height()
{
    return _height;
}

/* ========== Private: writeCommand() ========== */
void ST7789V::writeCommand(uint8_t cmd)
{
    gpio_set_level((gpio_num_t)ST7789V_PIN_DC, 0);  // DC low = command

    spi_transaction_t t = {};
    t.length = 8;
    t.tx_data[0] = cmd;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_transmit(_spi_dev, &t);
}

/* ========== Private: writeData() ========== */
void ST7789V::writeData(uint8_t data)
{
    gpio_set_level((gpio_num_t)ST7789V_PIN_DC, 1);  // DC high = data

    spi_transaction_t t = {};
    t.length = 8;
    t.tx_data[0] = data;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_transmit(_spi_dev, &t);
}

/* ========== Private: writeData16() ========== */
void ST7789V::writeData16(uint16_t data)
{
    gpio_set_level((gpio_num_t)ST7789V_PIN_DC, 1);  // DC high = data

    spi_transaction_t t = {};
    t.length = 16;
    // ST7789V 期望高字节在前 (MSB first)
    t.tx_data[0] = (data >> 8) & 0xFF;  // 高字节
    t.tx_data[1] = data & 0xFF;         // 低字节
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_transmit(_spi_dev, &t);
}

/* ========== Private: reset() ========== */
void ST7789V::reset()
{
    gpio_set_level((gpio_num_t)ST7789V_PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)ST7789V_PIN_RES, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)ST7789V_PIN_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}
