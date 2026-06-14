#include "led_control.h"
#include "cct_table.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <inttypes.h>

static const char *TAG = "led_ctrl";

uint16_t g_colorTemp  = CCT_DEFAULT;
uint8_t  g_brightness = BRT_DEFAULT;

/* ================================================================
 *  led_init
 *  EN:  LEDC Timer0/Ch0  @ 15kHz
 *  W/Y: MCPWM0 Timer0   @ 32kHz 死区互补
 *       A(GPIO10)=暖白正相, B(GPIO11)=冷白反相
 *       死区 200ns 双向保护
 * ================================================================ */
void led_init(void)
{
    ESP_LOGI(TAG, "led_init (EN=LEDC, WY=MCPWM legacy API)");

    /* ---- EN: LEDC ---- */
    ledc_timer_config_t et = {};
    et.speed_mode = LEDC_LOW_SPEED_MODE;
    et.duty_resolution = LEDC_TIMER_10_BIT;
    et.timer_num = LEDC_TIMER_0;
    et.freq_hz = EN_PWM_FREQ;
    et.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&et));

    ledc_channel_config_t ec = {};
    ec.gpio_num = PIN_EN_PWM;
    ec.speed_mode = LEDC_LOW_SPEED_MODE;
    ec.channel = LEDC_CHANNEL_0;
    ec.timer_sel = LEDC_TIMER_0;
    ec.duty = 0;
    ec.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ec));
    ESP_LOGI(TAG, "EN LEDC ok (GPIO=%d, %dHz)", PIN_EN_PWM, EN_PWM_FREQ);

    /* ---- W/Y: MCPWM 死区互补 (旧版 API) ---- */
    /*
     * MCPWM0, Timer0, Operator A/B
     * A(GPIO10, W): 正相 → TEZ=HIGH, CMP=LOW
     * B(GPIO11, Y): 反相 → TEZ=LOW,  CMP=HIGH
     * 死区 200ns 双向 (FED=上升沿, RED=下降沿)
     *
     * 注意: 旧版 MCPWM API 在 ESP32-S3 上通过内部兼容层工作,
     *       不会触发 new driver 的死锁 Bug
     */
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_WPWM);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PIN_YPWM);
    ESP_LOGI(TAG, "MCPWM GPIO configured (W=%d, Y=%d)", PIN_WPWM, PIN_YPWM);

    mcpwm_config_t mc = {};
    mc.frequency    = WY_PWM_FREQ;          /* 32000 Hz */
    mc.cmpr_a       = 0.0f;                 /* A 初始 0% */
    mc.cmpr_b       = 0.0f;                 /* B 初始 0% */
    mc.duty_mode    = MCPWM_DUTY_MODE_0;    /* A 高有效 */
    mc.counter_mode = MCPWM_UP_COUNTER;
    ESP_ERROR_CHECK(mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &mc));
    ESP_LOGI(TAG, "MCPWM init ok (%dHz)", WY_PWM_FREQ);

    /* B 设为反相: MCPWM_DUTY_MODE_1 = 低有效 (互补) */
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_1);

    /* 死区: 200ns 双向保护
     * APB_CLK = 80MHz → 200ns ≈ 16 ticks
     * MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE: B = ~A + 死区延迟
     * red/fed = 16 ticks (200ns) */
    ESP_ERROR_CHECK(mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
        MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 16, 16));
    ESP_LOGI(TAG, "MCPWM complementary + dead-time (200ns)");

    ESP_LOGI(TAG, "led_init done (EN=%d W=%d Y=%d, %dHz DT=%dns)",
             PIN_EN_PWM, PIN_WPWM, PIN_YPWM, WY_PWM_FREQ, WY_DEAD_TIME_NS);
}

/* ================================================================
 *  led_update
 *  EN   = (0~100%) → 0~1023
 *  W/Y  = CCT 查表 (0~1023) → A duty = w_val/10.23%
 *  B 反相, 占空比自动互补
 * ================================================================ */
void led_update(void)
{
    /* EN */
    uint32_t ed = (uint32_t)g_brightness * 1023 / 100;
    if (ed > 1023) ed = 1023;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, ed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    /* W/Y */
    uint8_t  idx = cct_to_index(g_colorTemp);
    uint16_t w   = CCT_PWM_TABLE[idx][0];   /* 0~1023 */
    float    pct = (float)w * 100.0f / 1023.0f;

    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pct);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, pct);
    /* B 是反相模式，同样的 compare 值 → 输出互补 (100-pct)% */

    static uint32_t log_cnt = 0;
    if (++log_cnt >= 20) {
        log_cnt = 0;
        ESP_LOGI(TAG, "LED_UPDATE: BRT=%d%% EN_duty=%" PRIu32 " W_duty=%.1f%% CCT=%dK",
                 g_brightness, ed, pct, g_colorTemp);
    }
}

/* ================================================================
 *  led_set_cct / led_set_brightness
 * ================================================================ */
void led_set_cct(uint16_t cct)
{
    g_colorTemp = cct;
    led_update();
}

void led_set_brightness(uint8_t brt)
{
    g_brightness = brt;
    led_update();
}

/* ================================================================
 *  fan_init / fan_update  (LEDC Timer1/Ch1)
 * ================================================================ */
void fan_init(void)
{
    ledc_timer_config_t ft = {};
    ft.speed_mode = LEDC_LOW_SPEED_MODE;
    ft.duty_resolution = LEDC_TIMER_8_BIT;
    ft.timer_num = LEDC_TIMER_1;
    ft.freq_hz = FAN_PWM_FREQ;
    ft.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ft));

    ledc_channel_config_t fc = {};
    fc.gpio_num = PIN_FAN_PWM;
    fc.speed_mode = LEDC_LOW_SPEED_MODE;
    fc.channel = LEDC_CHANNEL_1;
    fc.timer_sel = LEDC_TIMER_1;
    fc.duty = 0;
    fc.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&fc));

    gpio_config_t fe = {};
    fe.pin_bit_mask = (1ULL << PIN_FAN_EN);
    fe.mode = GPIO_MODE_OUTPUT;
    gpio_config(&fe);
    gpio_set_level((gpio_num_t)PIN_FAN_EN, 0);
}

void fan_update(float t)
{
    if (t < (float)FAN_ON_TEMP) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        gpio_set_level((gpio_num_t)PIN_FAN_EN, 0);
        return;
    }
    gpio_set_level((gpio_num_t)PIN_FAN_EN, 1);

    uint32_t d;
    if (t >= (float)FAN_FULL_TEMP) d = 255;
    else {
        float r = (t - (float)FAN_ON_TEMP) /
                  ((float)FAN_FULL_TEMP - (float)FAN_ON_TEMP);
        d = (uint32_t)(32.0f + r * (255.0f - 32.0f));
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, d);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ================================================================
 *  buzzer
 * ================================================================ */
void buzzer_init(void)
{
    gpio_config_t c = {};
    c.pin_bit_mask = (1ULL << PIN_BUZZER);
    c.mode = GPIO_MODE_OUTPUT;
    gpio_config(&c);
    gpio_set_level((gpio_num_t)PIN_BUZZER, 0);
}

void buzzer_beep(uint16_t freq, uint16_t ms)
{
    if (freq < 200) freq = 200;
    if (freq > 8000) freq = 8000;
    uint32_t h = 500000 / freq;
    uint32_t end = esp_timer_get_time() + (uint32_t)ms * 1000;
    while (esp_timer_get_time() < end) {
        gpio_set_level((gpio_num_t)PIN_BUZZER, 1);
        esp_rom_delay_us(h);
        gpio_set_level((gpio_num_t)PIN_BUZZER, 0);
        esp_rom_delay_us(h);
    }
}
