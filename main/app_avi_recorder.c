#include "app_avi_recorder.h"
#include "app_sd_card.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <strings.h>

static const char *TAG = "avi_recorder";

typedef struct {
    FILE *file;
    char filename[128];
    uint32_t frame_count;
    uint32_t total_bytes;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    bool is_recording;
    bool is_stopping;  // Flag to indicate stop is in progress
    long movi_list_pos;
    long idx1_pos;
    SemaphoreHandle_t mutex;  // Mutex for thread-safe file operations
} avi_recorder_t;

static avi_recorder_t recorder = {0};

// AVI file header structures
#pragma pack(push, 1)
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;
    char avi[4];            // "AVI "
} avi_riff_header_t;

typedef struct {
    char list[4];           // "LIST"
    uint32_t size;
    char type[4];
} avi_list_header_t;

typedef struct {
    char avih[4];           // "avih"
    uint32_t size;
    uint32_t microsec_per_frame;
    uint32_t max_bytes_per_sec;
    uint32_t padding;
    uint32_t flags;
    uint32_t total_frames;
    uint32_t initial_frames;
    uint32_t streams;
    uint32_t suggested_buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t reserved[4];
} avi_main_header_t;

typedef struct {
    char strh[4];           // "strh"
    uint32_t size;
    char type[4];           // "vids"
    char handler[4];        // "MJPG"
    uint32_t flags;
    uint16_t priority;
    uint16_t language;
    uint32_t initial_frames;
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
    uint32_t suggested_buffer_size;
    uint32_t quality;
    uint32_t sample_size;
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } frame;
} avi_stream_header_t;

typedef struct {
    char strf[4];           // "strf"
    uint32_t size;
    uint32_t size2;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bit_count;
    char compression[4];    // "MJPG"
    uint32_t image_size;
    uint32_t x_pels_per_meter;
    uint32_t y_pels_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;
} avi_stream_format_t;
#pragma pack(pop)

static esp_err_t write_avi_header(void)
{
    if (!recorder.file) return ESP_FAIL;

    fseek(recorder.file, 0, SEEK_SET);

    // RIFF header
    avi_riff_header_t riff = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = recorder.total_bytes + 4096,  // Will update later
        .avi = {'A', 'V', 'I', ' '}
    };
    fwrite(&riff, sizeof(riff), 1, recorder.file);

    // hdrl LIST
    // Size = 4 (type) + 64 (avih) + 124 (strl LIST: 12 header + 112 content) = 192
    avi_list_header_t hdrl_list = {
        .list = {'L', 'I', 'S', 'T'},
        .size = 192,  // type(4) + avih(64) + strl(124)
        .type = {'h', 'd', 'r', 'l'}
    };
    fwrite(&hdrl_list, sizeof(hdrl_list), 1, recorder.file);

    // avih - Main AVI header
    avi_main_header_t avih = {
        .avih = {'a', 'v', 'i', 'h'},
        .size = 56,
        .microsec_per_frame = 1000000 / recorder.fps,
        .max_bytes_per_sec = 500000,
        .padding = 0,
        .flags = 0x10,  // AVIF_HASINDEX
        .total_frames = recorder.frame_count,
        .initial_frames = 0,
        .streams = 1,
        .suggested_buffer_size = 1024 * 1024,
        .width = recorder.width,
        .height = recorder.height,
        .reserved = {0}
    };
    fwrite(&avih, sizeof(avih), 1, recorder.file);

    // strl LIST (stream list)
    // Size = 4 (type) + 64 (strh) + 48 (strf) = 116
    avi_list_header_t strl_list = {
        .list = {'L', 'I', 'S', 'T'},
        .size = 116,  // type(4) + strh(64) + strf(48)
        .type = {'s', 't', 'r', 'l'}
    };
    fwrite(&strl_list, sizeof(strl_list), 1, recorder.file);

    // strh - Stream header
    avi_stream_header_t strh = {
        .strh = {'s', 't', 'r', 'h'},
        .size = 56,
        .type = {'v', 'i', 'd', 's'},
        .handler = {'M', 'J', 'P', 'G'},
        .flags = 0,
        .priority = 0,
        .language = 0,
        .initial_frames = 0,
        .scale = 1,
        .rate = recorder.fps,
        .start = 0,
        .length = recorder.frame_count,
        .suggested_buffer_size = 1024 * 1024,
        .quality = 10000,
        .sample_size = 0,
        .frame = {0, 0, (int16_t)recorder.width, (int16_t)recorder.height}
    };
    fwrite(&strh, sizeof(strh), 1, recorder.file);

    // strf - Stream format
    avi_stream_format_t strf = {
        .strf = {'s', 't', 'r', 'f'},
        .size = 40,
        .size2 = 40,
        .width = recorder.width,
        .height = recorder.height,
        .planes = 1,
        .bit_count = 24,
        .compression = {'M', 'J', 'P', 'G'},
        .image_size = recorder.width * recorder.height * 3,
        .x_pels_per_meter = 0,
        .y_pels_per_meter = 0,
        .clr_used = 0,
        .clr_important = 0
    };
    fwrite(&strf, sizeof(strf), 1, recorder.file);

    // movi LIST
    avi_list_header_t movi_list = {
        .list = {'L', 'I', 'S', 'T'},
        .size = recorder.total_bytes + 4,  // Will update later
        .type = {'m', 'o', 'v', 'i'}
    };
    recorder.movi_list_pos = ftell(recorder.file);
    fwrite(&movi_list, sizeof(movi_list), 1, recorder.file);

    return ESP_OK;
}

esp_err_t avi_recorder_start(uint32_t width, uint32_t height, uint32_t fps)
{
    if (recorder.is_recording) {
        ESP_LOGW(TAG, "Already recording");
        return ESP_ERR_INVALID_STATE;
    }

    if (!app_sd_card_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    // Create mutex if not already created
    if (!recorder.mutex) {
        recorder.mutex = xSemaphoreCreateMutex();
        if (!recorder.mutex) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_FAIL;
        }
    }

    // Generate unique filename by scanning existing files
    uint32_t file_number = 0;
    DIR *dir = opendir(app_sd_card_get_mount_point());
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Check if filename matches "vidN.avi" pattern
            if (strncasecmp(entry->d_name, "vid", 3) == 0) {
                char *endptr;
                unsigned long num = strtoul(entry->d_name + 3, &endptr, 10);
                if (endptr && strcasecmp(endptr, ".avi") == 0) {
                    if (num >= file_number) {
                        file_number = num + 1;
                    }
                }
            }
        }
        closedir(dir);
    }
    
    snprintf(recorder.filename, sizeof(recorder.filename), 
             "%s/vid%lu.avi",
             app_sd_card_get_mount_point(),
             (unsigned long)file_number);
    ESP_LOGI(TAG, "Using filename: %s (next available number)", recorder.filename);

    // Check if file already exists (shouldn't happen with unique numbering)
    struct stat st;
    if (stat(recorder.filename, &st) == 0) {
        ESP_LOGW(TAG, "File already exists, will overwrite: %s", recorder.filename);
    }
    
    ESP_LOGI(TAG, "Creating file: %s", recorder.filename);
    
    // Use "wb" mode to create new file
    recorder.file = fopen(recorder.filename, "wb");
    if (!recorder.file) {
        ESP_LOGE(TAG, "Failed to create file with 'wb' mode: %s (errno=%d: %s)", 
                 recorder.filename, errno, strerror(errno));
        
        // Try to write a test file to check SD card
        char test_file[128];
        snprintf(test_file, sizeof(test_file), "%s/test.txt", app_sd_card_get_mount_point());
        FILE *test = fopen(test_file, "w");
        if (test) {
            fprintf(test, "SD card write test\n");
            fclose(test);
            ESP_LOGI(TAG, "Test file created successfully: %s", test_file);
            unlink(test_file);  // Delete test file
        } else {
            ESP_LOGE(TAG, "Failed to create test file: %s (errno=%d: %s)", 
                     test_file, errno, strerror(errno));
        }
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "File opened successfully in write mode");

    recorder.width = width;
    recorder.height = height;
    recorder.fps = fps;
    recorder.frame_count = 0;
    recorder.total_bytes = 0;
    recorder.is_stopping = false;
    recorder.is_recording = true;

    // Write initial header (will be updated later)
    ESP_LOGI(TAG, "Writing AVI header...");
    esp_err_t header_ret = write_avi_header();
    if (header_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write AVI header");
        fclose(recorder.file);
        recorder.file = NULL;
        recorder.is_recording = false;
        return header_ret;
    }
    ESP_LOGI(TAG, "AVI header written successfully");

    ESP_LOGI(TAG, "Recording started: %s", recorder.filename);
    return ESP_OK;
}

esp_err_t avi_recorder_write_frame(const uint8_t *data, size_t size)
{
    if (!recorder.is_recording || !recorder.file) {
        return ESP_ERR_INVALID_STATE;
    }

    // Don't write new frames if stopping is in progress
    if (recorder.is_stopping) {
        return ESP_ERR_INVALID_STATE;
    }

    // Lock mutex for thread-safe file operation
    if (xSemaphoreTake(recorder.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for frame write");
        return ESP_ERR_TIMEOUT;
    }

    // Double-check state after acquiring mutex
    if (!recorder.is_recording || !recorder.file || recorder.is_stopping) {
        xSemaphoreGive(recorder.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // Write frame chunk header "00dc" (compressed video frame)
    char chunk_id[4] = {'0', '0', 'd', 'c'};
    uint32_t chunk_size = size;
    
    size_t written = 0;
    written += fwrite(chunk_id, 4, 1, recorder.file);
    written += fwrite(&chunk_size, 4, 1, recorder.file);
    
    // Write the actual frame data
    size_t frame_written = fwrite(data, 1, size, recorder.file);
    if (frame_written != size) {
        ESP_LOGE(TAG, "Failed to write frame data: wrote %lu/%lu bytes", 
                 (unsigned long)frame_written, (unsigned long)size);
    }
    
    // Add padding if size is odd
    if (size & 1) {
        uint8_t pad = 0;
        fwrite(&pad, 1, 1, recorder.file);
        recorder.total_bytes += 1;
    }

    recorder.frame_count++;
    recorder.total_bytes += (8 + size);
    
    // Periodic flush and log every 30 frames
    if (recorder.frame_count % 30 == 0) {
        fflush(recorder.file);
        ESP_LOGI(TAG, "Recording: %lu frames, %lu bytes (avg %lu bytes/frame)", 
                 (unsigned long)recorder.frame_count, 
                 (unsigned long)recorder.total_bytes,
                 (unsigned long)(recorder.total_bytes / recorder.frame_count));
    }

    xSemaphoreGive(recorder.mutex);
    return ESP_OK;
}

esp_err_t avi_recorder_stop(void)
{
    if (!recorder.is_recording || !recorder.file) {
        return ESP_ERR_INVALID_STATE;
    }

    // Set stopping flag to prevent new frames from being written
    recorder.is_stopping = true;
    
    ESP_LOGI(TAG, "Stopping recording, waiting for pending frames...");
    
    // Wait a bit for any pending frame writes to complete
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait 100ms for pending operations
    
    // Now acquire mutex to safely finalize the file
    if (xSemaphoreTake(recorder.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for stop operation");
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGI(TAG, "Finalizing recording: %u frames, %u bytes", 
             recorder.frame_count, recorder.total_bytes);

    // Update RIFF file size
    fseek(recorder.file, 4, SEEK_SET);
    uint32_t file_size = recorder.total_bytes + 4096;
    fwrite(&file_size, 4, 1, recorder.file);

    // Update movi LIST size
    fseek(recorder.file, recorder.movi_list_pos + 4, SEEK_SET);
    uint32_t movi_size = recorder.total_bytes + 4;
    fwrite(&movi_size, 4, 1, recorder.file);

    // Update header with correct frame count
    write_avi_header();

    // Flush any remaining data
    fflush(recorder.file);

    fclose(recorder.file);
    recorder.file = NULL;
    recorder.is_recording = false;
    recorder.is_stopping = false;

    xSemaphoreGive(recorder.mutex);

    ESP_LOGI(TAG, "Recording stopped: %s (%u frames, %u bytes)", 
             recorder.filename, recorder.frame_count, recorder.total_bytes);

    return ESP_OK;
}

bool avi_recorder_is_recording(void)
{
    return recorder.is_recording;
}

uint32_t avi_recorder_get_frame_count(void)
{
    return recorder.frame_count;
}

