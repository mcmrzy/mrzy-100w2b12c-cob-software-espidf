#include "app_console.h"
#include "config.h"
#include "led_control.h"
#include "sensors.h"
#include "storage.h"
#include "sleep.h"
#include "ota.h"

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_CONSOLE";

/* ========== Command: status ========== */
static int cmd_status(int argc, char **argv) {
    printf("\n===== CobLux Status =====\n");
    printf("CCT:       %u K\n", g_colorTemp);
    printf("Brightness:%u %%\n", g_brightness);
    printf("Auto Brt:  %s\n", g_auto_brightness ? "ON" : "OFF");
    printf("LED Temp:  %.1f C\n", g_sensors.temperature);
    printf("Env Temp:  %.1f C\n", g_sensors.env_temperature);
    printf("Pressure:  %.0f hPa\n", g_sensors.pressure);
    printf("Bus:       %.1f V / %.2f A / %.1f W\n",
           g_sensors.bus_voltage, g_sensors.current_in, g_sensors.power_in);
    printf("Bat:       %.1f V / %.2f A / %.1f W\n",
           g_sensors.bat_voltage, g_sensors.current_bat, g_sensors.power_bat);
    printf("OTA Part:  %s\n", ota_get_running_partition());
    printf("Free Heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("===========================\n\n");
    return 0;
}

static int cmd_cct(int argc, char **argv) {
    if (argc < 2) { printf("Usage: cct <2700-6500>\n"); return 1; }
    int val = atoi(argv[1]);
    if (val < 2700 || val > 6500) { printf("Error: CCT must be 2700-6500 K\n"); return 1; }
    led_set_cct((uint16_t)val);
    g_settings.colorTemp = (uint16_t)val;
    storage_save(g_settings);
    printf("CCT: %d K\n", val);
    return 0;
}

static int cmd_brt(int argc, char **argv) {
    if (argc < 2) { printf("Usage: brt <0-100>\n"); return 1; }
    int val = atoi(argv[1]);
    if (val < 0 || val > 100) { printf("Error: brightness must be 0-100\n"); return 1; }
    led_set_brightness((uint8_t)val);
    g_settings.brightness = (uint8_t)val;
    storage_save(g_settings);
    printf("Brightness: %d %%\n", val);
    return 0;
}

static int cmd_scene(int argc, char **argv) {
    if (argc < 2) { printf("Usage: scene <1-4>\n"); return 1; }
    int idx = atoi(argv[1]) - 1;
    if (idx < 0 || idx >= 4) { printf("Error: scene must be 1-4\n"); return 1; }
    led_set_cct(SCENES[idx][0]);
    led_set_brightness(SCENES[idx][1]);
    printf("Scene %d: %u K @ %u %%\n", idx+1, SCENES[idx][0], SCENES[idx][1]);
    return 0;
}

static int cmd_reboot(int argc, char **argv) {
    printf("Rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return 0;
}

static int cmd_sleep(int argc, char **argv) {
    uint32_t sec = (argc > 1) ? (uint32_t)atoi(argv[1]) : 0;
    printf("Entering deep sleep (wake on key)...\n");
    deep_sleep_enter(sec, PIN_EN_KEY);
    return 0;
}

static int cmd_ota(int argc, char **argv) {
    if (argc < 2) { printf("Usage: ota <url>\n"); return 1; }
    ota_update(argv[1]);
    return 0;
}

/* ========== Register ========== */
static const esp_console_cmd_t commands[] = {
    {.command = "status", .help = "Show system status", .func = cmd_status, .func_w_context = NULL, .context = NULL},
    {.command = "cct",    .help = "Set color temp (2700-6500)", .func = cmd_cct, .func_w_context = NULL, .context = NULL},
    {.command = "brt",    .help = "Set brightness (0-100)", .func = cmd_brt, .func_w_context = NULL, .context = NULL},
    {.command = "scene",  .help = "Set scene (1-4)", .func = cmd_scene, .func_w_context = NULL, .context = NULL},
    {.command = "reboot", .help = "Reboot device", .func = cmd_reboot, .func_w_context = NULL, .context = NULL},
    {.command = "sleep",  .help = "Deep sleep [seconds]", .func = cmd_sleep, .func_w_context = NULL, .context = NULL},
    {.command = "ota",    .help = "OTA update <url>", .func = cmd_ota, .func_w_context = NULL, .context = NULL},
};

void app_console_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = {};
    repl_cfg.prompt = "cob-led> ";
    repl_cfg.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_cfg = {};
    uart_cfg.channel = 0;
    uart_cfg.baud_rate = 115200;

    esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl);

    for (int i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
        esp_console_cmd_register(&commands[i]);
    }

    esp_console_start_repl(repl);

    ESP_LOGI(TAG, "Console initialized on UART0. Type 'help'.");
}
