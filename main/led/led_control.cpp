#include "led_control.h"
#include "cct_table.h"

#include "driver/ledc.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "led_ctrl";

/* ---------- 全局变量 ---------- */
uint16_t g_colorTemp  = CCT_DEFAULT;
uint8_t  g_brightness = BRT_DEFAULT;

/* ---------- MCPWM 句柄 ---------- */
static mcpwm_cmpr_handle_t mcpwm_comparator_w = NULL;
static mcpwm_cmpr_handle_t mcpwm_comparator_y = NULL;

/* ================================================================
 *  led_init
 * ================================================================ */
void led_init(void)
{
    ESP_LOGI(TAG, "led_init start");

    /* ---- EN: LEDC Timer0 / Channel0 ---- */
    ledc_timer_config_t en_timer_cfg = {};
    en_timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    en_timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
    en_timer_cfg.timer_num       = LEDC_TIMER_0;
    en_timer_cfg.freq_hz         = EN_PWM_FREQ;
    en_timer_cfg.clk_cfg         = LEDC_AUTO_CLK;
    en_timer_cfg.deconfigure     = false;
    ESP_ERROR_CHECK(ledc_timer_config(&en_timer_cfg));

    ledc_channel_config_t en_ch_cfg = {};
    en_ch_cfg.gpio_num   = PIN_EN_PWM;
    en_ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    en_ch_cfg.channel    = LEDC_CHANNEL_0;
    en_ch_cfg.intr_type  = LEDC_INTR_DISABLE;
    en_ch_cfg.timer_sel  = LEDC_TIMER_0;
    en_ch_cfg.duty       = 0;
    en_ch_cfg.hpoint     = 0;
    en_ch_cfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    en_ch_cfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&en_ch_cfg));

    /* ---- W/Y: MCPWM Group0 新 API ---- */
    ESP_LOGI(TAG, "MCPWM new API init (W=%d Y=%d)", PIN_WPWM, PIN_YPWM);

    // 1) Timer — 字段顺序: group_id, clk_src, resolution_hz, count_mode, period_ticks, intr_priority, flags
    mcpwm_timer_config_t timer_cfg = {};
    timer_cfg.group_id      = 0;
    timer_cfg.clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timer_cfg.resolution_hz = WY_PWM_FREQ * 1024;
    timer_cfg.count_mode    = MCPWM_TIMER_COUNT_MODE_UP;
    timer_cfg.period_ticks  = 1024;
    timer_cfg.intr_priority = 0;
    mcpwm_timer_handle_t timer = NULL;
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &timer));
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // 2) Operator
    mcpwm_operator_config_t oper_cfg = {};
    oper_cfg.group_id = 0;
    mcpwm_oper_handle_t oper = NULL;
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 3) Comparator W — 字段顺序: intr_priority, flags
    mcpwm_comparator_config_t cmp_cfg = {};
    cmp_cfg.intr_priority = 0;
    cmp_cfg.flags.update_cmp_on_tez = 1;
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmp_cfg, &mcpwm_comparator_w));
    mcpwm_comparator_set_compare_value(mcpwm_comparator_w, 0);

    // 4) Comparator Y
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmp_cfg, &mcpwm_comparator_y));
    mcpwm_comparator_set_compare_value(mcpwm_comparator_y, 0);

    // 5) Generator W (暖白 PWM)
    mcpwm_generator_config_t gen_w_cfg = {};
    gen_w_cfg.gen_gpio_num = PIN_WPWM;
    mcpwm_gen_handle_t gen_w = NULL;
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_w_cfg, &gen_w));

    // 6) Generator Y (冷白 PWM)
    mcpwm_generator_config_t gen_y_cfg = {};
    gen_y_cfg.gen_gpio_num = PIN_YPWM;
    mcpwm_gen_handle_t gen_y = NULL;
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_y_cfg, &gen_y));

    // 7) 互补输出
    //    W: 高电平在 comparator match, 低电平在 timer zero
    //    Y: 低电平在 comparator match, 高电平在 timer zero  (互补)
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_w,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, mcpwm_comparator_w,
                                       MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_w,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY,
                                      MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_y,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, mcpwm_comparator_y,
                                       MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_y,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY,
                                      MCPWM_GEN_ACTION_HIGH)));

    // 8) 死区 — mcpwm_dead_time_config_t: posedge_delay_ticks, negedge_delay_ticks, flags
    //    注意: mcpwm_generator_set_dead_time 需要 3 个参数 (in_gen, out_gen, config)
    mcpwm_dead_time_config_t db_cfg = {};
    db_cfg.posedge_delay_ticks = 61;
    db_cfg.negedge_delay_ticks = 61;
    db_cfg.flags.invert_output = 0;
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gen_w, gen_w, &db_cfg));
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gen_y, gen_y, &db_cfg));

    ESP_LOGI(TAG, "led_init done");
}

/* ================================================================
 *  led_update
 * ================================================================ */
void led_update(void)
{
    /* EN 通道: 直接映射亮度到 10-bit 占空比 */
    uint32_t en_duty = (uint32_t)g_brightness * 1023 / 255;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, en_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    /* W/Y 通道: 通过 CCT 查表获取互补占空比 */
    uint8_t idx = cct_to_index(g_colorTemp);
    uint16_t w_val = CCT_PWM_TABLE[idx][0];
    uint16_t y_val = CCT_PWM_TABLE[idx][1];

    mcpwm_comparator_set_compare_value(mcpwm_comparator_w, w_val);
    mcpwm_comparator_set_compare_value(mcpwm_comparator_y, y_val);
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
 *  fan_init
 * ================================================================ */
void fan_init(void)
{
    ESP_LOGI(TAG, "fan_init start");

    /* Fan PWM: LEDC Timer3 / Channel2 */
    ledc_timer_config_t fan_timer_cfg = {};
    fan_timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    fan_timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    fan_timer_cfg.timer_num       = LEDC_TIMER_3;
    fan_timer_cfg.freq_hz         = FAN_PWM_FREQ;
    fan_timer_cfg.clk_cfg         = LEDC_AUTO_CLK;
    fan_timer_cfg.deconfigure     = false;
    ESP_ERROR_CHECK(ledc_timer_config(&fan_timer_cfg));

    ledc_channel_config_t fan_ch_cfg = {};
    fan_ch_cfg.gpio_num   = PIN_FAN_PWM;
    fan_ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    fan_ch_cfg.channel    = LEDC_CHANNEL_2;
    fan_ch_cfg.intr_type  = LEDC_INTR_DISABLE;
    fan_ch_cfg.timer_sel  = LEDC_TIMER_3;
    fan_ch_cfg.duty       = 0;
    fan_ch_cfg.hpoint     = 0;
    fan_ch_cfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    fan_ch_cfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&fan_ch_cfg));

    /* Fan 使能 */
    gpio_config_t fan_en_cfg = {};
    fan_en_cfg.pin_bit_mask = (1ULL << PIN_FAN_EN);
    fan_en_cfg.mode         = GPIO_MODE_OUTPUT;
    fan_en_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    fan_en_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    fan_en_cfg.intr_type    = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&fan_en_cfg));
    gpio_set_level((gpio_num_t)PIN_FAN_EN, 0);

    ESP_LOGI(TAG, "fan_init done");
}

/* ================================================================
 *  fan_update
 * ================================================================ */
void fan_update(float temp_c)
{
    if (temp_c < (float)FAN_ON_TEMP) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
        gpio_set_level((gpio_num_t)PIN_FAN_EN, 0);
        return;
    }

    gpio_set_level((gpio_num_t)PIN_FAN_EN, 1);

    uint32_t duty;
    if (temp_c >= (float)FAN_FULL_TEMP) {
        duty = 255;
    } else {
        float ratio = (temp_c - (float)FAN_ON_TEMP) / ((float)FAN_FULL_TEMP - (float)FAN_ON_TEMP);
        duty = (uint32_t)(32.0f + ratio * (255.0f - 32.0f));
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}
