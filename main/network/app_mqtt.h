#ifndef APP_MQTT_H
#define APP_MQTT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 全局 MQTT 启用标志 */
extern bool g_mqtt_enabled;

/* MQTT 初始化和循环 */
void mqtt_init(void);
void mqtt_loop(void);

/* 发布传感器数据到 MQTT */
void mqtt_publish_sensors(void);

/* 发布状态变更到 MQTT */
void mqtt_publish_status(const char *event);

#ifdef __cplusplus
}
#endif

#endif /* APP_MQTT_H */
