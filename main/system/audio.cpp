#include "audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO";

/* I2S pins — change as needed */
#define I2S_BCK_PIN   47
#define I2S_WS_PIN    21
#define I2S_DOUT_PIN  14

static i2s_chan_handle_t i2s_tx = NULL;

/* ========== Sine wave table (256 samples) ========== */
static int16_t sine_table[256];
static bool table_generated = false;

static void generate_sine_table(void)
{
    if (table_generated) return;
    for (int i = 0; i < 256; i++) {
        sine_table[i] = (int16_t)(32767.0f * sinf(2.0f * 3.14159f * i / 256.0f));
    }
    table_generated = true;
}

bool audio_init(void)
{
    ESP_LOGI(TAG, "I2S audio init (BCK=%d WS=%d DOUT=%d)",
             I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);

    generate_sine_table();

    /* I2S channel config */
    i2s_chan_config_t chan_cfg = {};
    chan_cfg.id = I2S_NUM_0;
    chan_cfg.role = I2S_ROLE_MASTER;
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 240;
    chan_cfg.auto_clear = true;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* Standard mode config */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = 16,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCK_PIN,
            .ws = (gpio_num_t)I2S_WS_PIN,
            .dout = (gpio_num_t)I2S_DOUT_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(i2s_tx, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S std mode failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = i2s_channel_enable(i2s_tx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S enable failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "I2S audio ready");
    return true;
}

void audio_play_tone(int freq_hz, int duration_ms)
{
    if (!i2s_tx) return;

    /* Generate buffer for this frequency */
    int samples_needed = (16000 * duration_ms) / 1000;
    int16_t *buf = (int16_t*)malloc(samples_needed * sizeof(int16_t));
    if (!buf) return;

    float phase_step = (float)freq_hz * 256.0f / 16000.0f;
    float phase = 0;

    for (int i = 0; i < samples_needed; i++) {
        int idx = ((int)phase) & 0xFF;
        buf[i] = sine_table[idx] / 2; /* Half volume */
        phase += phase_step;
    }

    size_t written = 0;
    i2s_channel_write(i2s_tx, buf, samples_needed * sizeof(int16_t), &written, portMAX_DELAY);
    free(buf);
}

void audio_say(const char *text)
{
    if (!i2s_tx || !text) return;

    ESP_LOGI(TAG, "Say: %s", text);

    /* Simple beep-based "speech": different tones for different characters */
    for (int i = 0; text[i]; i++) {
        if (text[i] == ' ') {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        /* Map character range to frequency */
        int freq = 400 + (text[i] % 26) * 60;
        audio_play_tone(freq, 30);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
