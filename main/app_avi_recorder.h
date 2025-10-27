#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start AVI recording
 * @param width Video width
 * @param height Video height
 * @param fps Frames per second
 * @return ESP_OK on success
 */
esp_err_t avi_recorder_start(uint32_t width, uint32_t height, uint32_t fps);

/**
 * @brief Write a MJPEG frame to AVI file
 * @param data MJPEG frame data
 * @param size Frame size in bytes
 * @return ESP_OK on success
 */
esp_err_t avi_recorder_write_frame(const uint8_t *data, size_t size);

/**
 * @brief Stop AVI recording
 * @return ESP_OK on success
 */
esp_err_t avi_recorder_stop(void);

/**
 * @brief Check if currently recording
 * @return true if recording, false otherwise
 */
bool avi_recorder_is_recording(void);

/**
 * @brief Get current frame count
 * @return Number of frames recorded
 */
uint32_t avi_recorder_get_frame_count(void);

#ifdef __cplusplus
}
#endif


