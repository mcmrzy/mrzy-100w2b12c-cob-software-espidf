#include "lvgl_disp.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "LVGL_DISP";

/* ========== Static Variables ========== */
static ST7789V *_tft = nullptr;                     ///< Pointer to ST7789V instance
static lv_disp_draw_buf_t draw_buf;                  ///< LVGL draw buffer descriptor
static lv_color_t buf1[LVGL_DISP_BUF_SIZE];          ///< Single draw buffer
static lv_disp_drv_t disp_drv;                       ///< LVGL display driver

/* ========== Display Flush Callback ========== */
static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    // Set the address window and push pixel data via DMA
    _tft->setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    _tft->pushColors((uint16_t *)color_p, w * h);

    // Tell LVGL flushing is done
    lv_disp_flush_ready(disp_drv);
}

/* ========== lvgl_disp_init() ========== */
void lvgl_disp_init(ST7789V *display)
{
    if (!display) {
        ESP_LOGE(TAG, "display pointer is NULL");
        return;
    }

    _tft = display;

    // Initialize the draw buffer (single buffer mode)
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, LVGL_DISP_BUF_SIZE);

    // Initialize the display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = _tft->width();
    disp_drv.ver_res = _tft->height();
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.direct_mode = 0;
    disp_drv.full_refresh = 0;
    disp_drv.sw_rotate = 0;
    disp_drv.antialiasing = 1;

    // Register the display driver
    lv_disp_drv_register(&disp_drv);

    // Set default theme with blue primary and red secondary
    lv_theme_default_init(nullptr, lv_palette_main(LV_PALETTE_BLUE),
                          lv_palette_main(LV_PALETTE_RED), true,
                          LV_FONT_DEFAULT);

    ESP_LOGI(TAG, "LVGL display driver initialized (%dx%d)", disp_drv.hor_res, disp_drv.ver_res);
}
