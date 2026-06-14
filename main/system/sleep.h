#ifndef SLEEP_H
#define SLEEP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enter deep sleep mode
 * @param seconds Wake up after N seconds (0 = never, wait for GPIO)
 * @param wakeup_pin GPIO pin to wake up on (low level trigger)
 * @note Pin must be RTC-capable GPIO
 */
void deep_sleep_enter(uint32_t seconds, int wakeup_pin);

#ifdef __cplusplus
}
#endif

#endif /* SLEEP_H */
