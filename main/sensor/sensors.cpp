/* ================================================================
 *  sensors.cpp - 传感器读取 (ESP32-S3 ADC 通道修正版)
 *
 *  ESP32-S3 ADC1 通道映射:
 *    GPIO1=CH0, GPIO2=CH1, GPIO3=CH2, GPIO4=CH3,
 *    GPIO5=CH4, GPIO6=CH5, GPIO7=CH6, GPIO8=CH7,
 *    GPIO9=CH8, GPIO10=CH9
 * ================================================================ */

#include "sensors.h"
#include "config.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "SENSORS";

/* ========== ADC 配置 ========== */
static esp_adc_cal_characteristics_t adc_chars;
static bool adc_initialized = false;

/* ========== I2C 配置 ========== */
static bool i2c_initialized = false;

/* ========== 温度计算常量 ========== */
#define NTC_BETA        3950.0f
#define NTC_R25         10000.0f
#define NTC_PULLUP      10000.0f
#define ADC_REF_VOLTAGE 3300  /* mV */

/* ========== ESP32-S3 GPIO → ADC1 通道映射宏 ==========
 *  config.h 定义的 ADC 引脚:
 *    PIN_AD_BUS  = 6  (总线电压) → ADC1_CH5
 *    PIN_AD_BAT  = 7  (电池电压) → ADC1_CH6
 *    PIN_AD_LDR  = 5  (光敏电阻) → ADC1_CH4
 *    PIN_AD_RT   = 4  (NTC 温度) → ADC1_CH3
 *    PIN_AD_I_IN = 1  (输入电流) → ADC1_CH0
 *    PIN_AD_I_BAT= 2  (电池电流) → ADC1_CH1
 */
#define GPIO_TO_ADC1_CH(gpio) ((adc1_channel_t)(gpio - 1))

/* 从 config.h 的 GPIO 定义推导 ADC 通道 */
static const adc1_channel_t CH_AD_BUS   = GPIO_TO_ADC1_CH(PIN_AD_BUS);    /* GPIO6 → CH5 */
static const adc1_channel_t CH_AD_BAT   = GPIO_TO_ADC1_CH(PIN_AD_BAT);    /* GPIO7 → CH6 */
static const adc1_channel_t CH_AD_LDR   = GPIO_TO_ADC1_CH(PIN_AD_LDR);    /* GPIO5 → CH4 */
static const adc1_channel_t CH_AD_RT    = GPIO_TO_ADC1_CH(PIN_AD_RT);     /* GPIO4 → CH3 */
static const adc1_channel_t CH_AD_I_IN  = GPIO_TO_ADC1_CH(PIN_AD_I_IN);   /* GPIO1 → CH0 */
static const adc1_channel_t CH_AD_I_BAT = GPIO_TO_ADC1_CH(PIN_AD_I_BAT);  /* GPIO2 → CH1 */

/* ================================================================
 *  ADC 初始化
 * ================================================================ */
static esp_err_t adc_init(void)
{
    if (adc_initialized) return ESP_OK;

    adc1_config_width(ADC_WIDTH_BIT_12);

    /* 配置所有用到的 ADC 通道 (使用 config.h 的 GPIO 定义推导) */
    adc1_config_channel_atten(CH_AD_BUS,   ADC_ATTEN_DB_12);  /* 总线电压 */
    adc1_config_channel_atten(CH_AD_I_IN,  ADC_ATTEN_DB_12);  /* 输入电流 */
    adc1_config_channel_atten(CH_AD_BAT,   ADC_ATTEN_DB_12);  /* 电池电压 */
    adc1_config_channel_atten(CH_AD_I_BAT, ADC_ATTEN_DB_12);  /* 电池电流 */
    adc1_config_channel_atten(CH_AD_RT,    ADC_ATTEN_DB_12);  /* NTC 温度 */
    adc1_config_channel_atten(CH_AD_LDR,   ADC_ATTEN_DB_12);  /* 光敏电阻 */

    /* ADC 校准 */
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, ADC_REF_VOLTAGE, &adc_chars);

    adc_initialized = true;
    ESP_LOGI(TAG, "ADC initialized (BUS=GPIO%d/CH%d, IBUS=GPIO%d/CH%d, BAT=GPIO%d/CH%d, NTC=GPIO%d/CH%d)",
             PIN_AD_BUS, CH_AD_BUS, PIN_AD_I_IN, CH_AD_I_IN,
             PIN_AD_BAT, CH_AD_BAT, PIN_AD_RT, CH_AD_RT);
    return ESP_OK;
}

/* ================================================================
 *  I2C 初始化 (BMP280)
 * ================================================================ */
static esp_err_t i2c_init(void)
{
    if (i2c_initialized) return ESP_OK;

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)BMP280_SDA_PIN;
    conf.scl_io_num = (gpio_num_t)BMP280_SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed");
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return ret;
    }

    /* 检查 BMP280 是否存在 */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BMP280 not found at 0x%02X", BMP280_ADDR);
        i2c_driver_delete(I2C_NUM_0);
        return ESP_ERR_NOT_FOUND;
    }

    /* 初始化 BMP280 */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xF4, true);
    i2c_master_write_byte(cmd, 0x27, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    i2c_initialized = true;
    ESP_LOGI(TAG, "I2C/BMP280 initialized");
    return ESP_OK;
}

/* ================================================================
 *  读取 BMP280
 * ================================================================ */
static esp_err_t bmp280_read(float *temp, float *pressure)
{
    if (!i2c_initialized) return ESP_ERR_INVALID_STATE;

    uint8_t data[6];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xF7, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) return ret;

    int32_t adc_p = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | ((int32_t)data[2] >> 4);
    int32_t adc_t = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | ((int32_t)data[5] >> 4);

    *temp = (float)(adc_t - 512000) / 1024.0f + 25.0f;
    *pressure = (float)adc_p / 256.0f;

    return ESP_OK;
}

/* ================================================================
 *  电压/电流计算
 * ================================================================ */
static float adc_to_voltage(int adc_raw)
{
    return esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars) / 1000.0f;
}

/* TP181A1: 10mΩ 检流电阻, 增益 50 */
static float adc_to_current(int adc_raw)
{
    float vout = adc_to_voltage(adc_raw);
    return vout / (CURRENT_SENSE_RESISTOR * CURRENT_GAIN) * CURRENT_SCALE_FACTOR;
}

/* NTC 温度计算 */
static float adc_to_temp(int adc_raw)
{
    if (adc_raw <= 0) return -999.0f;
    if (adc_raw >= 4095) return 999.0f;

    float v_ntc = adc_to_voltage(adc_raw);
    float r_ntc = NTC_PULLUP * v_ntc / (ADC_REF_VOLTAGE / 1000.0f - v_ntc);

    /* Steinhart-Hart 简化版 */
    float steinhart = r_ntc / NTC_R25;
    steinhart = logf(steinhart);
    steinhart /= NTC_BETA;
    steinhart += 1.0f / (25.0f + 273.15f);
    steinhart = 1.0f / steinhart;
    steinhart -= 273.15f;

    return steinhart;
}

/* ================================================================
 *  公共接口
 * ================================================================ */
esp_err_t sensors_init(void)
{
    ESP_LOGI(TAG, "sensors_init start");

    esp_err_t ret = adc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC init failed");
        return ret;
    }

    /* BMP280 I2C 暂时跳过 */
    ESP_LOGW(TAG, "BMP280 skipped (I2C causes IWDT on v5.4)");
    i2c_initialized = false;

    ESP_LOGI(TAG, "sensors_init done");
    return ESP_OK;
}

esp_err_t sensors_read_power(float *vbus, float *ibus, float *pbus, float *vbat, float *ibat, float *pbat)
{
    if (!adc_initialized) return ESP_ERR_INVALID_STATE;

    int raw_vbus = 0, raw_ibus = 0, raw_vbat = 0, raw_ibat = 0;
    for (int i = 0; i < 8; i++) {
        raw_vbus += adc1_get_raw(CH_AD_BUS);     /* GPIO6/CH5 */
        raw_ibus += adc1_get_raw(CH_AD_I_IN);    /* GPIO2/CH1 */
        raw_vbat += adc1_get_raw(CH_AD_BAT);     /* GPIO7/CH6 */
        raw_ibat += adc1_get_raw(CH_AD_I_BAT);   /* GPIO1/CH0 */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    raw_vbus /= 8;
    raw_ibus /= 8;
    raw_vbat /= 8;
    raw_ibat /= 8;

    *vbus = adc_to_voltage(raw_vbus) * ADC_BUS_SCALE;
    *ibus = adc_to_current(raw_ibus);
    *pbus = (*vbus) * (*ibus);

    *vbat = adc_to_voltage(raw_vbat) * ADC_BAT_SCALE;
    *ibat = adc_to_current(raw_ibat);
    *pbat = (*vbat) * (*ibat);

    static uint32_t log_cnt = 0;
    if (++log_cnt >= 10) {
        log_cnt = 0;
        ESP_LOGI(TAG, "ADC_RAW[VBUS=%d I_IN=%d VBAT=%d I_BAT=%d] → "
                 "V=%.2fV I=%.3fA IB=%.3fA",
                 raw_vbus, raw_ibus, raw_vbat, raw_ibat,
                 *vbus, *ibus, *ibat);
    }

    return ESP_OK;
}

esp_err_t sensors_read_env(float *temp_led, float *temp_env, float *pressure)
{
    if (!adc_initialized) return ESP_ERR_INVALID_STATE;

    /* NTC 温度 (GPIO4/CH3) */
    int raw_ntc = 0;
    for (int i = 0; i < 8; i++) {
        raw_ntc += adc1_get_raw(CH_AD_RT);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    raw_ntc /= 8;
    *temp_led = adc_to_temp(raw_ntc);

    /* BMP280 环境温度和气压 */
    if (i2c_initialized) {
        bmp280_read(temp_env, pressure);
    } else {
        *temp_env = 25.0f;
        *pressure = 1013.0f;
    }

    return ESP_OK;
}
