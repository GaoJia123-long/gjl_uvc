#ifndef APP_BLE_H
#define APP_BLE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE peripheral functionality
 * @return ESP_OK on success
 */
esp_err_t app_ble_init(void);

/**
 * @brief Start BLE advertising
 * @return ESP_OK on success
 */
esp_err_t app_ble_start_advertising(void);

/**
 * @brief Stop BLE advertising
 * @return ESP_OK on success
 */
esp_err_t app_ble_stop_advertising(void);

#ifdef __cplusplus
}
#endif

#endif // APP_BLE_H