#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SD card pin definitions for ESP32-P4 (SDMMC 4-bit mode)
#define SDMMC_PIN_CMD       GPIO_NUM_44
#define SDMMC_PIN_CLK       GPIO_NUM_43
#define SDMMC_PIN_D0        GPIO_NUM_39
#define SDMMC_PIN_D1        GPIO_NUM_40
#define SDMMC_PIN_D2        GPIO_NUM_41
#define SDMMC_PIN_D3        GPIO_NUM_42

/**
 * @brief Initialize SD card
 * @return ESP_OK on success
 */
esp_err_t app_sd_card_init(void);

/**
 * @brief Deinitialize SD card
 * @return ESP_OK on success
 */
esp_err_t app_sd_card_deinit(void);

/**
 * @brief Check if SD card is mounted
 * @return true if mounted, false otherwise
 */
bool app_sd_card_is_mounted(void);

/**
 * @brief Get SD card mount point
 * @return Mount point path string
 */
const char* app_sd_card_get_mount_point(void);

#ifdef __cplusplus
}
#endif

