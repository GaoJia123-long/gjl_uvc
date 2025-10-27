#include "app_sd_card.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

static const char *TAG = "sd_card";
static sdmmc_card_t *card = NULL;
#if SOC_SDMMC_IO_POWER_EXTERNAL
static sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
#endif

#define MOUNT_POINT "/sdcard"

esp_err_t app_sd_card_init(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem (same as official example)
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Initializing SD card on Slot 0");
    ESP_LOGI(TAG, "Slot 1 will be available for ESP-Hosted/WiFi Remote");

    // Use SDMMC Slot 0 for SD card (SDMMC_HOST_SLOT_1 can be used for ESP-Hosted)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // Initialize LDO power control for Slot 0 (CRITICAL for ESP32-P4!)
#if SOC_SDMMC_IO_POWER_EXTERNAL
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,  // LDO channel 4 for Slot 0
    };
    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create on-chip LDO power control");
        return ret;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
    ESP_LOGI(TAG, "LDO power control initialized for Slot 0");
#endif

    // Slot configuration (IO MUX for Slot 0)
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.cd = SDMMC_SLOT_NO_CD;
    slot_config.wp = SDMMC_SLOT_NO_WP;
    slot_config.flags = 0;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t app_sd_card_deinit(void)
{
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card");
        return ret;
    }
    
#if SOC_SDMMC_IO_POWER_EXTERNAL
    // Deinitialize the power control driver
    if (pwr_ctrl_handle) {
        ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete on-chip LDO power control");
        }
        pwr_ctrl_handle = NULL;
    }
#endif
    
    ESP_LOGI(TAG, "SD card unmounted");
    card = NULL;
    return ESP_OK;
}

bool app_sd_card_is_mounted(void)
{
    return (card != NULL);
}

const char* app_sd_card_get_mount_point(void)
{
    return MOUNT_POINT;
}

