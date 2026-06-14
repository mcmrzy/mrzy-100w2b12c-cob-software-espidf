#ifndef ESPNOW_H
#define ESPNOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback type for received ESP-NOW commands */
typedef void (*espnow_recv_cb_t)(const char *data, int len);

/**
 * @brief Initialize ESP-NOW and register as broadcast receiver
 * @param callback Called when data received
 * @return true on success
 */
bool espnow_init(espnow_recv_cb_t callback);

/**
 * @brief Send data to the broadcast address (FF:FF:FF:FF:FF:FF)
 * @param data Data to send
 * @param len Length of data
 * @return true on success
 */
bool espnow_send(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_H */
