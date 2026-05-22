/**
 * @file ble_ui.h
 * @brief NimBLE BLE 控制界面
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

void ble_init(void);
void ble_notify(void);

#ifdef __cplusplus
}
#endif
