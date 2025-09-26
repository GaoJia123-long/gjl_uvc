#include "app_ble.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "bleprph.h"

static const char *TAG = "app_ble";

void ble_store_config_init(void);
// BLE主机任务
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// BLE同步回调
static void ble_on_sync(void)
{
    int rc;
    
    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin advertising. */
    rc = bleprph_start_advertising();
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
}

// BLE重置回调
static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset; reason=%d", reason);
}

esp_err_t app_ble_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing BLE with NimBLE...");

    /* Initialize NVS — it is used to store PHY calibration data */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d", ret);
        return ret;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Configure security settings */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;  // Just works pairing
    ble_hs_cfg.sm_bonding = 0;  // No bonding
    ble_hs_cfg.sm_mitm = 0;     // No MITM
    ble_hs_cfg.sm_sc = 0;       // No secure connections

    /* Initialize GATT server */
    int rc = gatt_svr_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize GATT server");
        return ESP_FAIL;
    }

    /* Set the device name */
    rc = ble_svc_gap_device_name_set("GJL-UVC-Camera");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name");
        return ESP_FAIL;
    }

    /* Initialize BLE store */
    ble_store_config_init();

    /* Start BLE host task */
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE initialization completed");
    return ESP_OK;
}

esp_err_t app_ble_start_advertising(void)
{
    return bleprph_start_advertising();
}

esp_err_t app_ble_stop_advertising(void)
{
    return bleprph_stop_advertising();
}