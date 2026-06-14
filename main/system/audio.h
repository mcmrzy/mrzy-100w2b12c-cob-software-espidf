#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize I2S audio output
 * @return true on success
 */
bool audio_init(void);

/**
 * @brief Play a tone (blocking)
 * @param freq_hz Frequency in Hz
 * @param duration_ms Duration in milliseconds
 */
void audio_play_tone(int freq_hz, int duration_ms);

/**
 * @brief Speak a text string using simple beep patterns
 * @param text Text to "speak" (morse-like beeping)
 */
void audio_say(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
