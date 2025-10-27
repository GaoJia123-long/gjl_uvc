#include "app_video_player.h"
#include "app_sd_card.h"
#include "app_uvc.h"
#include "app_lcd.h"
#include "app_avi_recorder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_video_dec.h"
#include "esp_video_dec_default.h"
#include "esp_video_codec_utils.h"
#include "avi_player.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

static const char *TAG = "video_player";

static lv_obj_t *file_list_screen = NULL;
static lv_obj_t *player_screen = NULL;
static lv_obj_t *parent_screen = NULL;

// Playback state
typedef struct {
    char filename[256];
    bool is_playing;
    lv_obj_t *canvas;
    lv_img_dsc_t img_dsc;
    uint16_t *frame_buffer_front;  // Buffer being displayed by LVGL
    uint16_t *frame_buffer_back;   // Buffer being written/decoded
    uint32_t buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    lv_obj_t *status_label;
    avi_player_handle_t avi_handle;
} playback_state_t;

static playback_state_t playback = {0};

// AVI player callback: video frame
static void video_frame_callback(frame_data_t *data, void *arg)
{
    static TickType_t last_display_time = 0;
    static uint32_t frame_rx_count = 0;
    static bool first_call = true;
    
    if (first_call) {
        ESP_LOGI(TAG, "First video frame callback received!");
        first_call = false;
    }
    
    if (!playback.is_playing || !data || data->type != FRAME_TYPE_VIDEO) {
        ESP_LOGW(TAG, "Frame callback rejected: playing=%d, data=%p, type=%d", 
                 playback.is_playing, data, data ? data->type : -1);
        return;
    }
    
    frame_rx_count++;
    
    if (frame_rx_count == 1) {
        ESP_LOGI(TAG, "Processing first frame...");
    }
    
    // First frame - initialize double buffers with proper alignment
    if (!playback.frame_buffer_front && data->video_info.width > 0 && data->video_info.height > 0) {
        playback.width = data->video_info.width;
        playback.height = data->video_info.height;
        
        // Get frame alignment requirement from decoder
        uint8_t in_frame_align = 0;
        uint8_t out_frame_align = 0;
        esp_video_dec_get_frame_align(video_dec_handle, &in_frame_align, &out_frame_align);
        ESP_LOGI(TAG, "Decoder requires output frame alignment: %d bytes", out_frame_align);
        
        // Allocate aligned double buffers for RGB565 output
        uint32_t requested_size = playback.width * playback.height * sizeof(uint16_t);
        uint32_t actual_size = 0;
        
        // Front buffer (displayed by LVGL)
        playback.frame_buffer_front = (uint16_t *)esp_video_codec_align_alloc(out_frame_align, requested_size, &actual_size);
        if (!playback.frame_buffer_front) {
            ESP_LOGE(TAG, "Failed to allocate front buffer");
            return;
        }
        memset(playback.frame_buffer_front, 0, actual_size);
        
        // Back buffer (for decoding)
        playback.frame_buffer_back = (uint16_t *)esp_video_codec_align_alloc(out_frame_align, requested_size, &actual_size);
        if (!playback.frame_buffer_back) {
            ESP_LOGE(TAG, "Failed to allocate back buffer");
            esp_video_codec_free(playback.frame_buffer_front);
            playback.frame_buffer_front = NULL;
            return;
        }
        memset(playback.frame_buffer_back, 0, actual_size);
        
        playback.buffer_size = actual_size;
        
        // Setup LVGL image descriptor pointing to front buffer
        playback.img_dsc.header.always_zero = 0;
        playback.img_dsc.header.w = playback.width;
        playback.img_dsc.header.h = playback.height;
        playback.img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        playback.img_dsc.data_size = actual_size;
        playback.img_dsc.data = (uint8_t *)playback.frame_buffer_front;
        
        ESP_LOGI(TAG, "Initialized double buffers: %dx%d, size=%u bytes each", 
                 playback.width, playback.height, actual_size);
    }
    
    if (!playback.frame_buffer_back || !video_dec_handle) {
        return;
    }
    
    // Frame rate limiting: ~15fps display (every other frame)
    TickType_t current_time = xTaskGetTickCount();
    if (current_time - last_display_time < pdMS_TO_TICKS(66)) {  // 66ms = ~15fps
        // Skip this frame
        if (frame_rx_count % 100 == 0) {
            ESP_LOGI(TAG, "Received %lu frames", frame_rx_count);
        }
        return;
    }
    last_display_time = current_time;
    
    // Decode MJPEG to back buffer (no lock needed)
    esp_video_dec_in_frame_t in_frame = {
        .data = data->data,
        .size = data->data_bytes,
    };
    
    esp_video_dec_out_frame_t out_frame = {
        .data = (uint8_t *)playback.frame_buffer_back,
        .size = playback.buffer_size,
        .decoded_size = 0,
    };
    
    esp_vc_err_t ret = esp_video_dec_process(video_dec_handle, &in_frame, &out_frame);
    if (ret == ESP_VC_ERR_OK) {
        // Swap byte order for RGB565 in back buffer (no lock needed)
        uint16_t *pixels = playback.frame_buffer_back;
        int pixel_count = playback.width * playback.height;
        for (int i = 0; i < pixel_count; i++) {
            pixels[i] = (pixels[i] >> 8) | (pixels[i] << 8);
        }
        
        // Now swap buffers and update display (very fast operation under lock)
        if (example_lvgl_lock(0)) {
            // Atomic buffer swap
            uint16_t *temp = playback.frame_buffer_front;
            playback.frame_buffer_front = playback.frame_buffer_back;
            playback.frame_buffer_back = temp;
            
            // Update image descriptor to point to new front buffer
            playback.img_dsc.data = (uint8_t *)playback.frame_buffer_front;
            
            if (playback.canvas) {
                lv_img_set_src(playback.canvas, &playback.img_dsc);
            }
            
            // Update frame counter
            playback.frame_count++;
            if (playback.status_label && playback.frame_count % 10 == 0) {
                lv_label_set_text_fmt(playback.status_label, "Frame: %lu", playback.frame_count);
            }
            
            example_lvgl_unlock();
            
            // Log progress
            if (playback.frame_count % 30 == 0) {
                ESP_LOGI(TAG, "Displayed %lu frames", playback.frame_count);
            }
        }
    } else {
        if (frame_rx_count % 50 == 0) {
            ESP_LOGW(TAG, "JPEG decode failed: %d (frame %lu)", ret, frame_rx_count);
        }
    }
}

// AVI player callback: playback ended
static void playback_end_callback(void *arg)
{
    ESP_LOGI(TAG, "Playback ended");
    playback.is_playing = false;
    
    // Update status
    if (example_lvgl_lock(100)) {
        if (playback.status_label) {
            lv_label_set_text(playback.status_label, "Playback complete");
        }
        example_lvgl_unlock();
    }
}

// Stop button callback
static void stop_button_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Stop button pressed");
    
    if (playback.avi_handle) {
        avi_player_play_stop(playback.avi_handle);
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    video_player_close();
}

// File list entry callback
static void file_item_click_cb(lv_event_t *e)
{
    const char *filename = (const char *)lv_event_get_user_data(e);
    
    if (filename) {
        ESP_LOGI(TAG, "Selected file: %s", filename);
        video_player_play_video(filename, parent_screen);
    }
}

// Back button callback
static void back_button_cb(lv_event_t *e)
{
    video_player_close();
}

void video_player_show_file_list(lv_obj_t *parent)
{
    if (!app_sd_card_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return;
    }

    parent_screen = parent;

    // NOTE: This function is called from LVGL button callback,
    // which already holds the LVGL mutex, so we don't need to take it again.
    // If called from outside LVGL context, the caller must hold the mutex.

    // Create file list screen
    file_list_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(file_list_screen, lv_color_black(), 0);

    // Title label
    lv_obj_t *title = lv_label_create(file_list_screen);
    lv_label_set_text(title, "Video Files");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(file_list_screen);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);
    lv_obj_add_event_cb(btn_back, back_button_cb, LV_EVENT_CLICKED, NULL);

    // Create scrollable list
    lv_obj_t *list = lv_list_create(file_list_screen);
    lv_obj_set_size(list, 380, 400);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x222222), 0);

    // Scan SD card for .avi files
    DIR *dir = opendir(app_sd_card_get_mount_point());
    if (dir) {
        struct dirent *entry;
        int file_count = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            // Check if file ends with .avi
            size_t len = strlen(entry->d_name);
            if (len > 4 && strcasecmp(entry->d_name + len - 4, ".avi") == 0) {
                // Create full path (larger buffer to avoid truncation)
                char *full_path = malloc(512);
                if (full_path) {
                    snprintf(full_path, 512, "%s/%s", app_sd_card_get_mount_point(), entry->d_name);
                    
                    // Get file size
                    struct stat st;
                    long file_size = 0;
                    if (stat(full_path, &st) == 0) {
                        file_size = st.st_size;
                    }
                    
                    // Create list item with filename and size (larger buffer for long filenames)
                    char label_text[300];
                    snprintf(label_text, sizeof(label_text), "%.240s\n%.1f MB", 
                             entry->d_name, file_size / 1024.0 / 1024.0);
                    
                    lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_VIDEO, label_text);
                    lv_obj_add_event_cb(btn, file_item_click_cb, LV_EVENT_CLICKED, full_path);
                    
                    file_count++;
                }
            }
        }
        closedir(dir);
        
        if (file_count == 0) {
            lv_obj_t *no_files_label = lv_label_create(list);
            lv_label_set_text(no_files_label, "No video files found");
            lv_obj_set_style_text_color(no_files_label, lv_color_hex(0x888888), 0);
            lv_obj_center(no_files_label);
        }
        
        ESP_LOGI(TAG, "Found %d video files", file_count);
    } else {
        ESP_LOGE(TAG, "Failed to open SD card directory");
    }

    // Load the file list screen
    lv_scr_load(file_list_screen);
}

void video_player_play_video(const char *filename, lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Playing video: %s", filename);
    
    // CRITICAL: Stop UVC camera stream to free the JPEG decoder
    // The hardware decoder cannot be shared between UVC and video player
    ESP_LOGI(TAG, "Stopping UVC stream to free decoder...");
    uvc_dev_info_t dev_info;
    if (app_uvc_get_dev_frame_info(0, &dev_info) == ESP_OK) {
        if (dev_info.if_streaming) {
            app_uvc_control_dev_by_index(0, false, 0);  // Stop streaming
            vTaskDelay(pdMS_TO_TICKS(200));  // Wait for stream to stop
            ESP_LOGI(TAG, "UVC stream stopped");
        }
    }
    
    // Stop any existing playback
    if (playback.is_playing && playback.avi_handle) {
        avi_player_play_stop(playback.avi_handle);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Clean up old resources
    if (playback.avi_handle) {
        avi_player_deinit(playback.avi_handle);
        playback.avi_handle = NULL;
    }
    if (playback.frame_buffer_front) {
        esp_video_codec_free(playback.frame_buffer_front);
        playback.frame_buffer_front = NULL;
    }
    if (playback.frame_buffer_back) {
        esp_video_codec_free(playback.frame_buffer_back);
        playback.frame_buffer_back = NULL;
    }
    
    // Reset playback state
    memset(&playback, 0, sizeof(playback));
    snprintf(playback.filename, sizeof(playback.filename), "%s", filename);
    
    // Initialize AVI player
    avi_player_config_t config = {
        .buffer_size = 100 * 1024,  // 100KB buffer
        .video_cb = video_frame_callback,
        .audio_cb = NULL,  // No audio support
        .audio_set_clock_cb = NULL,
        .avi_play_end_cb = playback_end_callback,
        .priority = 4,
        .coreID = 0,
        .user_data = NULL,
        .stack_size = 8192,
        .stack_in_psram = false,  // Reading from SD card, not flash
    };
    
    esp_err_t ret = avi_player_init(config, &playback.avi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AVI player: %d", ret);
        return;
    }
    
    // NOTE: Called from button callback which already holds LVGL mutex
    parent_screen = parent;
    
    // Create player screen
    player_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(player_screen, lv_color_black(), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(player_screen);
    const char *basename = strrchr(filename, '/');
    lv_label_set_text(title, basename ? basename + 1 : filename);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Status label
    playback.status_label = lv_label_create(player_screen);
    lv_label_set_text(playback.status_label, "Starting...");
    lv_obj_set_style_text_color(playback.status_label, lv_color_white(), 0);
    lv_obj_align(playback.status_label, LV_ALIGN_BOTTOM_MID, 0, -50);
    
    // Create canvas for video display
    playback.canvas = lv_img_create(player_screen);
    lv_obj_center(playback.canvas);
    
    // Stop button
    lv_obj_t *btn_stop = lv_btn_create(player_screen);
    lv_obj_set_size(btn_stop, 100, 40);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *label_stop = lv_label_create(btn_stop);
    lv_label_set_text(label_stop, "Stop");
    lv_obj_center(label_stop);
    lv_obj_add_event_cb(btn_stop, stop_button_cb, LV_EVENT_CLICKED, NULL);
    
    // Load player screen
    ESP_LOGI(TAG, "Loading player screen...");
    lv_scr_load(player_screen);
    ESP_LOGI(TAG, "Player screen loaded");
    
    // Start playback
    playback.is_playing = true;
    playback.frame_count = 0;
    
    ESP_LOGI(TAG, "Starting AVI player from file: %s", filename);
    // Start AVI player (this will call video_frame_callback for each frame)
    ret = avi_player_play_from_file(playback.avi_handle, filename);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start AVI playback: %d", ret);
        playback.is_playing = false;
    } else {
        ESP_LOGI(TAG, "AVI player play_from_file returned OK, waiting for callbacks...");
    }
}

void video_player_close(void)
{
    ESP_LOGI(TAG, "Closing video player...");
    
    // NOTE: Called from button callback which already holds LVGL mutex
    
    // Stop AVI player if active
    if (playback.avi_handle) {
        if (playback.is_playing) {
            // Release LVGL mutex temporarily to let player task finish
            example_lvgl_unlock();
            
            avi_player_play_stop(playback.avi_handle);
            vTaskDelay(pdMS_TO_TICKS(200));
            
            example_lvgl_lock(-1);
        }
        
        // Deinitialize player
        avi_player_deinit(playback.avi_handle);
        playback.avi_handle = NULL;
    }
    
    // Clean up display buffers (using aligned free)
    if (playback.frame_buffer_front) {
        esp_video_codec_free(playback.frame_buffer_front);
        playback.frame_buffer_front = NULL;
    }
    if (playback.frame_buffer_back) {
        esp_video_codec_free(playback.frame_buffer_back);
        playback.frame_buffer_back = NULL;
    }
    
    playback.is_playing = false;
    
    // Return to parent screen
    if (parent_screen) {
        lv_scr_load(parent_screen);
    }
    
    // Clean up file list screen
    if (file_list_screen) {
        lv_obj_del(file_list_screen);
        file_list_screen = NULL;
    }
    
    // Clean up player screen
    if (player_screen) {
        lv_obj_del(player_screen);
        player_screen = NULL;
    }
    
    // Restart UVC stream to resume camera preview
    ESP_LOGI(TAG, "Restarting UVC stream...");
    uvc_dev_info_t dev_info;
    if (app_uvc_get_dev_frame_info(0, &dev_info) == ESP_OK) {
        if (!dev_info.if_streaming) {
            app_uvc_control_dev_by_index(0, true, dev_info.active_resolution);
            ESP_LOGI(TAG, "UVC stream restarted");
        }
    }
    
    ESP_LOGI(TAG, "Video player closed");
}

