#include "sensors.h"
#include "bmp280.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "math.h"

static const char *TAG = "sensors";

/* ---------- ADC 通道映射 ---------- */
#define ADC_UNIT         ADC_UNIT_1
#define ADC_ATTEN        ADC_ATTEN_DB_12

#define ADC_CH_BUS       ADC_CHANNEL_5    // GPIO6
#define ADC_CH_BAT       ADC_CHANNEL_6    // GPIO7
#define ADC_CH_LDR       ADC_CHANNEL_4    // GPIO5
#define ADC_CH_NTC       ADC_CHANNEL_3    // GPIO4
#define ADC_CH_I_BAT     ADC_CHANNEL_0    // GPIO1
#define ADC_CH_I_IN      ADC_CHANNEL_1    // GPIO2

#define ADC_CHANNEL_COUNT 6

/* ---------- NTC 参数 (Steinhart) ---------- */
#define NTC_BETA          3435.0f
#define NTC_R0            10000.0f       // 25 C 时的阻值
#define NTC_T0            298.15f        // 25 C => Kelvin
#define NTC_VREF          3.3f
#define NTC_SERIES_R      10000.0f       // 分压电阻

/* ---------- 电流参数 (TP181A1 + 10mΩ 检流电阻) ---------- */
/*
 * TP181A1: 增益 = 50 V/V
 * Rshunt   = 10mΩ (0.01Ω)
 * 灵敏度   = 增益 × Rshunt = 50 × 0.01 = 0.5 V/A
 *
 * 电流 = (Vout - Vzero) / 灵敏度
 *      = (Vout - 1.65V) / 0.5
 *
 * 其中 1.65V = 3.3V / 2，为 TP181A1 双向输出的零点电压
 */
#define TP181_GAIN           50.0f         // TP181A1 增益 (A1=50, A2=100, A3=200)
#define SHUNT_R             0.01f         // 检流电阻 10mΩ
#define CURRENT_SENSITIVITY  (TP181_GAIN * SHUNT_R)  // 0.5 V/A
#define CURRENT_ZERO_V       1.65f         // 双向输出零点电压 (3.3V/2)

/* ---------- ADC 平均采样次数 ---------- */
#define ADC_SAMPLES       10

/* ---------- 全局变量 ---------- */
SensorData g_sensors = {};
bool       g_auto_brightness = true;

/* ---------- BMP280 可用标志 ---------- */
static bool bmp280_available = false;

/* ---------- ADC 句柄 ---------- */
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t         cali_handle = NULL;

/* ================================================================
 *  sensors_init
 * ================================================================ */
void sensors_init(void)
{
    ESP_LOGI(TAG, "sensors_init start");

    /* ADC1 单元初始化 */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    /* 配置所有通道: 12dB 衰减 */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten     = ADC_ATTEN,
        .bitwidth  = ADC_BITWIDTH_DEFAULT,
    };

    adc_channel_t channels[ADC_CHANNEL_COUNT] = {
        ADC_CH_BUS, ADC_CH_BAT, ADC_CH_LDR,
        ADC_CH_NTC, ADC_CH_I_BAT, ADC_CH_I_IN,
    };
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channels[i], &chan_cfg));
    }

    /* ADC 校准 (曲线拟合模式 - v5.4) */
    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id  = ADC_UNIT;
    cali_cfg.chan     = ADC_CH_BUS;  /* 使用第一个通道作为校准参考 */
    cali_cfg.atten    = ADC_ATTEN;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration scheme created");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed: %s, using raw readings", esp_err_to_name(ret));
        cali_handle = NULL;
    }

    /* 初始化 BMP280 (可选，失败不影响其他传感器) */
    bmp280_available = bmp280_init();
    if (bmp280_available) {
        ESP_LOGI(TAG, "BMP280 available for env sensing");
    } else {
        ESP_LOGW(TAG, "BMP280 not available, skipping env sensors");
    }

    ESP_LOGI(TAG, "sensors_init done");
}

/* ================================================================
 *  sensors_read
 * ================================================================ */
void sensors_read(void)
{
    adc_channel_t channels[ADC_CHANNEL_COUNT] = {
        ADC_CH_BUS, ADC_CH_BAT, ADC_CH_LDR,
        ADC_CH_NTC, ADC_CH_I_BAT, ADC_CH_I_IN,
    };

    int raw[ADC_CHANNEL_COUNT] = {0};
    int mv[ADC_CHANNEL_COUNT]  = {0};

    /* 多次采样取平均 */
    for (int s = 0; s < ADC_SAMPLES; s++) {
        for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
            int val;
            adc_oneshot_read(adc_handle, channels[i], &val);
            raw[i] += val;
        }
    }
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        raw[i] /= ADC_SAMPLES;
    }

    /* 转换为毫伏 */
    if (cali_handle) {
        for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
            adc_cali_raw_to_voltage(cali_handle, raw[i], &mv[i]);
        }
    } else {
        /* 无校准时粗略转换: 12dB 衰减, 默认 12-bit */
        for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
            mv[i] = (int)((float)raw[i] * NTC_VREF * 1000.0f / 4095.0f);
        }
    }

    /* 总线电压 (假设电阻分压, 11:1) */
    g_sensors.bus_voltage = (float)mv[0] / 1000.0f * 11.0f;

    /* 电池电压 (假设电阻分压, 2:1) */
    g_sensors.bat_voltage = (float)mv[1] / 1000.0f * 2.0f;

    /* LDR */
    g_sensors.ldr_raw = (uint16_t)raw[2];
    g_sensors.ldr_lux = (uint16_t)raw[2];  // 简化: 直接用原始值, 可后续加映射

    /* NTC 温度 */
    g_sensors.temperature = ntc_to_temp((uint16_t)raw[3]);

    /* 电池电流 (双向) */
    float v_i_bat = (float)mv[4] / 1000.0f;
    g_sensors.current_bat = (v_i_bat - CURRENT_ZERO_V) / CURRENT_SENSITIVITY;

    /* 输入电流 (双向) */
    float v_i_in = (float)mv[5] / 1000.0f;
    g_sensors.current_in = (v_i_in - CURRENT_ZERO_V) / CURRENT_SENSITIVITY;

    /* 功率 */
    g_sensors.power_bat = g_sensors.bat_voltage * g_sensors.current_bat;
    g_sensors.power_in  = g_sensors.bus_voltage * g_sensors.current_in;

    /* 读取 BMP280 环境数据 */
    if (bmp280_available) {
        bmp280_data_t bmp_data;
        if (bmp280_read(&bmp_data)) {
            g_sensors.env_temperature = bmp_data.temperature;
            g_sensors.pressure        = bmp_data.pressure;
            g_sensors.altitude        = bmp_data.altitude;
        }
    }
}

/* ================================================================
 *  ntc_to_temp  -- Steinhart 公式
 * ================================================================ */
float ntc_to_temp(uint16_t adc_val)
{
    /* ADC 值转电压 */
    float v_ntc = (float)adc_val * NTC_VREF / 4095.0f;

    /* 分压电路: Vcc -- R_series -- NTC -- GND */
    float r_ntc;
    if (v_ntc >= NTC_VREF - 0.01f) {
        r_ntc = NTC_SERIES_R * 100.0f;  // 防止除零
    } else {
        r_ntc = NTC_SERIES_R * (NTC_VREF - v_ntc) / v_ntc;
    }

    /* Steinhart-Hart 简化公式 (Beta 参数法) */
    float inv_t = 1.0f / NTC_T0 + logf(r_ntc / NTC_R0) / NTC_BETA;
    float temp_k = 1.0f / inv_t;
    float temp_c = temp_k - 273.15f;

    return temp_c;
}
