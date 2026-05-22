/**
 * @file web_ui.h
 * @brief WiFi + HTTP REST API 控制界面
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

void wifi_init(void);
void webui_init(void);

#ifdef __cplusplus
}
#endif
