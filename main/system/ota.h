#ifndef OTA_H
#define OTA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start OTA firmware update from HTTPS URL
 * @param url Full HTTPS URL to the firmware binary
 * @return true on success (reboots), false on failure
 */
bool ota_update(const char *url);

/**
 * @brief Get last OTA partition that booted
 * @return "ota_0" or "ota_1"
 */
const char* ota_get_running_partition(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H */
