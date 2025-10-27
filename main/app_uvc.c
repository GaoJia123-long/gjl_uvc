
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "usb/uvc_host.h"
#include "app_uvc.h"
#include "esp_timer.h"

#include "app_lcd.h"
#include "lvgl.h"
#include <string.h>
#include <stdlib.h>
#include "esp_video_dec.h"
#include "esp_video_dec_default.h"
#include "esp_video_codec_utils.h"
#include "app_avi_recorder.h"

// External handles and functions from app_lcd.c
extern esp_lcd_panel_handle_t panel_handle;

// LVGL canvas for displaying camera frame
static lv_obj_t *camera_canvas = NULL;
static lv_img_dsc_t camera_img_dsc;
static uint16_t *camera_frame_buffer = NULL;
static uint16_t *camera_frame_buffer_back = NULL;  // Back buffer for double buffering
static volatile bool buffer_ready = false;  // Flag to indicate buffer is ready to swap

// ESP_VIDEO_CODEC decoder handle (shared with video player)
esp_video_dec_handle_t video_dec_handle = NULL;

#define UVC_CAM_NUM             MAX_CAMERAS

#if CONFIG_SPIRAM
#define NUMBER_OF_FRAME_BUFFERS 6 // Number of frames from the camera
#else
#define NUMBER_OF_FRAME_BUFFERS 3 // Number of frames from the camera
#endif
#define JPG_MIN_SIZE 40000
#define JPG_COMPRESSION_RATIO 5
#define max(a, b) ((a) > (b) ? (a) : (b))
#define UVC_ESTIMATE_JPG_SIZE_B(width, height, bpp) \
    (max(((width) * (height) * (bpp) / 8) / JPG_COMPRESSION_RATIO, JPG_MIN_SIZE))
#define UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dwFrameInterval) (((dwFrameInterval) != 0) ? 10000000 / ((float)(dwFrameInterval)) : 0)

static const char *TAG = "uvc";

static const char *FORMAT_STR[] = {
    "FORMAT_UNDEFINED",
    "FORMAT_MJPEG",
    "FORMAT_YUY2",
    "FORMAT_H264",
    "FORMAT_H265",
};

#define EVENT_DISCONNECT ((1 << 2))
#define EVENT_START      ((1 << 3))
#define EVENT_STOP       ((1 << 4))

#define EVENT_ALL          (EVENT_DISCONNECT | EVENT_START | EVENT_STOP)
typedef struct uvc_dev_s {
    uint8_t index;
    uint16_t dev_addr;
    uint8_t uvc_stream_index;
    uint16_t vid;
    uint16_t pid;
    uvc_host_stream_hdl_t stream;
    EventGroupHandle_t event_group;
    QueueHandle_t frame_queue;
    bool if_streaming;
    int active_frame_index;                        // Only used for if_streaming is true
    int require_frame_index;
    size_t frame_info_num;
    SLIST_ENTRY(uvc_dev_s) list_entry;
    uvc_host_frame_info_t frame_info[];
} uvc_dev_t;

typedef struct {
    int uvc_dev_num;
    SLIST_HEAD(list_dev, uvc_dev_s) uvc_devices_list;
} uvc_dev_obj_t;

static uvc_dev_obj_t p_uvc_dev_obj = {0};

// Forward declarations
static esp_err_t uvc_open(uvc_dev_t *dev, int frame_index);
static esp_err_t init_video_decoder(void);
static void init_camera_canvas(int width, int height);
static void decode_and_display_frame(const uint8_t *jpg_data, size_t jpg_size);
static void frame_display_task(void *arg);

static void usb_lib_task(void *arg)
{
    // Install USB Host driver. Should only be called once in entire application
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    // Clean up USB Host
    vTaskDelay(100 / portTICK_PERIOD_MS); // Short delay to allow clients clean-up
    usb_host_uninstall();
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    vTaskDelete(NULL);
}

static void uvc_task(void *arg)
{
    uvc_dev_t *dev = (uvc_dev_t *)arg;
    dev->event_group = xEventGroupCreate();
    dev->frame_queue = xQueueCreate(2, sizeof(uvc_host_frame_t *));
    SLIST_INSERT_HEAD(&p_uvc_dev_obj.uvc_devices_list, dev, list_entry);
    p_uvc_dev_obj.uvc_dev_num++;

    // first open - select 320x240 resolution (Frame Index 10 in device descriptor)
    // Find the frame index for 320x240 resolution
    int target_frame_index = 0;
    bool found_target_res = false;
    ESP_LOGI(TAG, "Searching for 320x240 in %d MJPEG frames", dev->frame_info_num);
    for (int i = 0; i < dev->frame_info_num; i++) {
        ESP_LOGI(TAG, "  Frame[%d]: %dx%d", i, dev->frame_info[i].h_res, dev->frame_info[i].v_res);
        if (dev->frame_info[i].h_res == 320 && dev->frame_info[i].v_res == 240) {
            target_frame_index = i;
            found_target_res = true;
            ESP_LOGI(TAG, "✓ Found 320x240 resolution at index %d", i);
            break;
        }
    }
    if (!found_target_res) {
        ESP_LOGW(TAG, "320x240 not found, using index 0: %dx%d", 
                 dev->frame_info[0].h_res, dev->frame_info[0].v_res);
    }
    dev->require_frame_index = target_frame_index;
    ESP_LOGI(TAG, "Open the uvc device\n");
    ESP_ERROR_CHECK(uvc_open(dev, dev->require_frame_index));

    // Initialize video decoder (ESP_VIDEO_CODEC)
    if (init_video_decoder() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize video decoder");
        return;
    }

    // Initialize camera canvas for LCD display
    init_camera_canvas(dev->frame_info[target_frame_index].h_res, 
                      dev->frame_info[target_frame_index].v_res);

    // Create frame display task
    xTaskCreate(frame_display_task, "frame_display", 8192, dev, 4, NULL);

    // Auto-start streaming after initialization
    ESP_LOGI(TAG, "Auto-starting video stream...");
    xEventGroupSetBits(dev->event_group, EVENT_START);

    bool exit = false;
    while (!exit) 
    {
        EventBits_t uxBits = xEventGroupWaitBits(dev->event_group, EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

        if (uxBits & EVENT_START) 
        {
            if (dev->if_streaming) 
            {
                continue;
            }

            uvc_host_stream_format_t vs_format = 
            {
                .h_res = dev->frame_info[dev->require_frame_index].h_res,
                .v_res = dev->frame_info[dev->require_frame_index].v_res,
                .fps = UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dev->frame_info[dev->require_frame_index].default_interval),
                .format = dev->frame_info[dev->require_frame_index].format
            };
            ESP_ERROR_CHECK(uvc_host_stream_format_select(dev->stream, &vs_format));
            ESP_LOGI(TAG, "uvc begin to stream");
            ESP_ERROR_CHECK(uvc_host_stream_start(dev->stream));
            dev->if_streaming = true;
        }

        if (uxBits & EVENT_STOP) 
        {
            if (!dev->if_streaming) 
            {
                continue;
            }

            ESP_LOGI(TAG, "uvc end to stream");
            ESP_ERROR_CHECK(uvc_host_stream_stop(dev->stream));
            uvc_host_frame_t *frame = NULL;
            while (xQueueReceive(dev->frame_queue, &frame, 0) == pdTRUE) 
            {
                uvc_host_frame_return(dev->stream, frame);
            }
            dev->if_streaming = false;
        }

        if (uxBits & EVENT_DISCONNECT) 
        {
            exit = true;
        }
    }

    // Clean up
    uvc_host_frame_t *frame = NULL;
    while (xQueueReceive(dev->frame_queue, &frame, 0) == pdTRUE) {
        uvc_host_frame_return(dev->stream, frame);
    }
    uvc_host_stream_close(dev->stream);
    vQueueDelete(dev->frame_queue);
    SLIST_REMOVE(&p_uvc_dev_obj.uvc_devices_list, dev, uvc_dev_s, list_entry);
    p_uvc_dev_obj.uvc_dev_num--;
    free(dev);

    // Create a task to open the device
    // Open device make Hotplug
    vTaskDelete(NULL);
}

void driver_event_cb(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    uvc_dev_t *dev = NULL;

    switch (event->type) {
    case UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED: {
        ESP_LOGI(TAG, "Device connected");

        if (p_uvc_dev_obj.uvc_dev_num >= UVC_CAM_NUM) {
            ESP_LOGW(TAG, "Too many devices");
            break;
        }

        dev = (uvc_dev_t *)calloc(1, sizeof(uvc_dev_t) + (event->device_connected.frame_info_num) * sizeof(uvc_host_frame_info_t));
        assert(dev != NULL);

        dev->index = p_uvc_dev_obj.uvc_dev_num;
        dev->dev_addr = event->device_connected.dev_addr;
        dev->uvc_stream_index = event->device_connected.uvc_stream_index;
        dev->frame_info_num = event->device_connected.frame_info_num;
        ESP_LOGI(TAG, "Cam[%d] uvc_stream_index = %d", dev->index, dev->uvc_stream_index);

        uvc_host_frame_info_t *frame_info = (uvc_host_frame_info_t *)calloc(dev->frame_info_num, sizeof(uvc_host_frame_info_t));
        assert(frame_info);

        uvc_host_get_frame_list(dev->dev_addr, dev->uvc_stream_index, (uvc_host_frame_info_t (*)[])frame_info, &dev->frame_info_num);
        int pick_frame_info_num = 0;
        for (int i = 0; i < dev->frame_info_num; i++) {
            if (frame_info[i].format == UVC_VS_FORMAT_MJPEG) {
                dev->frame_info[pick_frame_info_num] = frame_info[i];
                pick_frame_info_num++;
                ESP_LOGI(TAG, "Pick Cam[%d] %s %d*%d@%.1ffps", dev->index, FORMAT_STR[frame_info[i].format], frame_info[i].h_res, frame_info[i].v_res, UVC_DESC_DWFRAMEINTERVAL_TO_FPS(frame_info[i].default_interval));
            } else {
                ESP_LOGI(TAG, "Drop Cam[%d] %s %d*%d@%.1ffps", dev->index, FORMAT_STR[frame_info[i].format], frame_info[i].h_res, frame_info[i].v_res, UVC_DESC_DWFRAMEINTERVAL_TO_FPS(frame_info[i].default_interval));
            }
        }
        // Use actual picked frame count instead of hardcoded 8
        dev->frame_info_num = pick_frame_info_num;
        ESP_LOGI(TAG, "Total picked MJPEG frames: %d", pick_frame_info_num);

        free(frame_info);
        if (pick_frame_info_num == 0) {
            ESP_LOGE(TAG, "Not supported FORMAT_MJPEG, destroy cam[%d]", dev->index);
            free(dev);
            dev = NULL;
            return;
        }
        break;
    }
    default:
        break;
    }

    xTaskCreate(uvc_task, "uvc_task", 4096, dev, 5, NULL);
}

void stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    uvc_dev_t *dev = (uvc_dev_t *)user_ctx;
    switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGE(TAG, "Cam[%d] USB error", dev->index);
        break;
    case UVC_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(TAG, "Cam[%d] disconnected", dev->index);
        xEventGroupSetBits(dev->event_group, EVENT_DISCONNECT);
        break;
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "Cam[%d] Frame buffer overflow", dev->index);
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGD(TAG, "Cam[%d] Frame buffer underflow", dev->index);
        break;
    default:
        abort();
        break;
    }
}




// Initialize LVGL canvas for displaying camera frames
static void init_camera_canvas(int width, int height)
{
    if (camera_canvas) {
        return; // Already initialized
    }

    // Get frame alignment requirement from decoder
    uint8_t in_frame_align = 0;
    uint8_t out_frame_align = 0;
    esp_video_dec_get_frame_align(video_dec_handle, &in_frame_align, &out_frame_align);
    ESP_LOGI(TAG, "Decoder requires output frame alignment: %d bytes", out_frame_align);

    // Allocate aligned frame buffers for RGB565 format (2 bytes per pixel) - double buffering
    uint32_t buffer_size = width * height * sizeof(uint16_t);
    uint32_t actual_size = 0;
    
    // Front buffer (displayed)
    camera_frame_buffer = (uint16_t *)esp_video_codec_align_alloc(out_frame_align, buffer_size, &actual_size);
    if (!camera_frame_buffer) {
        ESP_LOGE(TAG, "Failed to allocate camera frame buffer");
        return;
    }
    memset(camera_frame_buffer, 0, actual_size);
    
    // Back buffer (for decoding)
    camera_frame_buffer_back = (uint16_t *)esp_video_codec_align_alloc(out_frame_align, buffer_size, &actual_size);
    if (!camera_frame_buffer_back) {
        ESP_LOGE(TAG, "Failed to allocate camera back buffer");
        esp_video_codec_free(camera_frame_buffer);
        camera_frame_buffer = NULL;
        return;
    }
    memset(camera_frame_buffer_back, 0, actual_size);
    
    ESP_LOGI(TAG, "Allocated double buffers: size=%u bytes each", actual_size);
    buffer_size = actual_size;  // Use actual allocated size

    // Setup image descriptor
    camera_img_dsc.header.always_zero = 0;
    camera_img_dsc.header.w = width;
    camera_img_dsc.header.h = height;
    camera_img_dsc.data_size = buffer_size;
    camera_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    camera_img_dsc.data = (uint8_t *)camera_frame_buffer;

    // Create LVGL canvas/image object
    ESP_LOGI(TAG, "Attempting to lock LVGL for canvas creation...");
    if (example_lvgl_lock(1000)) {
        ESP_LOGI(TAG, "LVGL lock acquired, creating canvas...");
        camera_canvas = lv_img_create(lv_scr_act());
        lv_img_set_src(camera_canvas, &camera_img_dsc);
        lv_obj_center(camera_canvas);
        example_lvgl_unlock();
        ESP_LOGI(TAG, "Camera canvas initialized: %dx%d", width, height);
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for canvas creation!");
    }
}

// Initialize ESP_VIDEO_CODEC decoder
static esp_err_t init_video_decoder(void)
{
    if (video_dec_handle) {
        return ESP_OK; // Already initialized
    }

    // Register default decoders (includes hardware MJPEG decoder)
    esp_video_dec_register_default();

    // Configure decoder for MJPEG to RGB565_LE
    esp_video_dec_cfg_t dec_cfg = {
        .codec_type = ESP_VIDEO_CODEC_TYPE_MJPEG,
        .out_fmt = ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_LE,  // Little-endian RGB565 for LVGL
    };

    // Open decoder (will automatically use HW decoder if available)
    esp_vc_err_t ret = esp_video_dec_open(&dec_cfg, &video_dec_handle);
    if (ret != ESP_VC_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open video decoder: %d", ret);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Video decoder initialized successfully");
    return ESP_OK;
}

// Decode MJPEG frame and display on LCD using ESP_VIDEO_CODEC with double buffering
static void decode_and_display_frame(const uint8_t *jpg_data, size_t jpg_size)
{
    if (!camera_frame_buffer || !camera_frame_buffer_back) {
        ESP_LOGW(TAG, "Camera frame buffers not initialized");
        return;
    }

    if (!video_dec_handle) {
        ESP_LOGW(TAG, "Video decoder not initialized");
        return;
    }

    // Decode to back buffer (not displayed yet) - no lock needed for decoding
    esp_video_dec_in_frame_t in_frame = {
        .data = (uint8_t *)jpg_data,
        .size = jpg_size,
    };

    esp_video_dec_out_frame_t out_frame = {
        .data = (uint8_t *)camera_frame_buffer_back,
        .size = camera_img_dsc.data_size,
        .decoded_size = 0,
    };

    // Decode MJPEG to RGB565 in back buffer
    esp_vc_err_t ret = esp_video_dec_process(video_dec_handle, &in_frame, &out_frame);
    if (ret == ESP_VC_ERR_OK) {
        // Swap RGB565 byte order (LE <-> BE conversion) in back buffer
        uint16_t *pixels = camera_frame_buffer_back;
        int pixel_count = camera_img_dsc.header.w * camera_img_dsc.header.h;
        for (int i = 0; i < pixel_count; i++) {
            pixels[i] = (pixels[i] >> 8) | (pixels[i] << 8);
        }
        
        // Now lock LVGL only for buffer swap (minimal lock time)
        if (example_lvgl_lock(0)) {
            // Perform atomic buffer swap
            uint16_t *temp = camera_frame_buffer;
            camera_frame_buffer = camera_frame_buffer_back;
            camera_frame_buffer_back = temp;
            
            // Update image descriptor to point to new front buffer
            camera_img_dsc.data = (uint8_t *)camera_frame_buffer;
            lv_img_set_src(camera_canvas, &camera_img_dsc);
            
            // Buffer swap complete - unlock LVGL immediately
            example_lvgl_unlock();
        }
    } else {
        ESP_LOGW(TAG, "JPEG decode failed: %d", ret);
    }
}

// Task to process frames from queue
static void frame_display_task(void *arg)
{
    uvc_dev_t *dev = (uvc_dev_t *)arg;
    uvc_host_frame_t *frame = NULL;
    TickType_t last_frame_time = 0;
    const TickType_t min_frame_interval = pdMS_TO_TICKS(33); // ~30fps limit

    ESP_LOGI(TAG, "Frame display task started");

    while (1) {
        if (xQueueReceive(dev->frame_queue, &frame, portMAX_DELAY) == pdTRUE) {
            TickType_t current_time = xTaskGetTickCount();
            
            // Frame rate limiting to prevent tearing
            if (current_time - last_frame_time >= min_frame_interval) {
                if (frame && frame->vs_format.format == UVC_VS_FORMAT_MJPEG) {
                    // Decode and display the MJPEG frame
                    decode_and_display_frame(frame->data, frame->data_len);
                    
                    // If recording, save MJPEG frame (non-blocking - copy to recorder's buffer)
                    // Note: This should be quick, actual file write happens in recorder's internal task
                    if (avi_recorder_is_recording()) {
                        // TODO: Optimize - currently synchronous write may cause lag
                        avi_recorder_write_frame(frame->data, frame->data_len);
                    }
                    
                    last_frame_time = current_time;
                }
            }
            
            // Return frame back to UVC driver
            uvc_host_frame_return(dev->stream, frame);
        }
    }
}

bool frame_callback(const uvc_host_frame_t *frame, void *user_ctx)
{

    uvc_dev_t *dev = (uvc_dev_t *)user_ctx;
    switch (frame->vs_format.format) {
    case UVC_VS_FORMAT_YUY2:
    case UVC_VS_FORMAT_H264:
    case UVC_VS_FORMAT_H265:
    case UVC_VS_FORMAT_MJPEG:
        //ESP_LOGI(TAG, "Cam[%d] Frame received, size: %d", dev->index, frame->data_len);
        break;
    default:
        ESP_LOGI(TAG, "Unsupported format!");
        break;
    }
    if (xQueueSend(dev->frame_queue, &frame, 0) != pdPASS) {
        // drop frame
        return true;
    }

    //new
 

    //new

    // If we return false from this callback, we must return the frame with uvc_host_frame_return(stream, frame);
    return false;
}



static esp_err_t uvc_open(uvc_dev_t *dev, int frame_index)
{
    const uvc_host_stream_config_t stream_config = {
        .event_cb = stream_callback,
        .frame_cb = frame_callback,
        .user_ctx = dev,
        .usb = {
            .dev_addr = dev->dev_addr,
            .vid = 0,
            .pid = 0,
            .uvc_stream_index = dev->uvc_stream_index,
        },
        .vs_format = {
            .h_res = dev->frame_info[frame_index].h_res,
            .v_res = dev->frame_info[frame_index].v_res,
            .fps = UVC_DESC_DWFRAMEINTERVAL_TO_FPS(dev->frame_info[frame_index].default_interval),
            .format = dev->frame_info[frame_index].format,
        },
        .advanced = {
            .number_of_frame_buffers = NUMBER_OF_FRAME_BUFFERS,
            .frame_size = UVC_ESTIMATE_JPG_SIZE_B(dev->frame_info[frame_index].h_res, dev->frame_info[frame_index].v_res, 16),
#if CONFIG_SPIRAM
            .frame_heap_caps = MALLOC_CAP_SPIRAM,
#else
            .frame_heap_caps = 0,
#endif
            .number_of_urbs = 3,
            .urb_size = 4 * 1024,
        },
    };

    ESP_LOGI(TAG, "Opening the UVC[%d]...", dev->index);
    ESP_LOGI(TAG, "frame_size.advanced.frame_size: %d", stream_config.advanced.frame_size);
    esp_err_t err = uvc_host_stream_open(&stream_config, pdMS_TO_TICKS(5000), &dev->stream);
    if (ESP_OK != err) {
        ESP_LOGI(TAG, "Failed to open UVC[%d]", dev->index);
        return ESP_FAIL;
    }
#if CONFIG_PRINTF_CAMERA_USB_DESC
    uvc_host_desc_print(dev->stream);
#endif


    dev->active_frame_index = frame_index;
    return ESP_OK;
}

esp_err_t app_uvc_init(void)
{
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 15, NULL);
    assert(task_created == pdPASS);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // UVC driver install
    const uvc_host_driver_config_t default_driver_config = {
        .driver_task_stack_size = 5 * 1024,
        .driver_task_priority = 5,
        .xCoreID = 1,
        .create_background_task = true,
        .event_cb = driver_event_cb,
    };
    ESP_ERROR_CHECK(uvc_host_install(&default_driver_config));
    return ESP_OK;
}

esp_err_t app_uvc_control_dev_by_index(int index, bool if_open, int resolution_index)
{
    ESP_RETURN_ON_FALSE(index < UVC_CAM_NUM, ESP_ERR_INVALID_ARG, TAG, "Invalid index");

    uvc_dev_t *dev;
    SLIST_FOREACH(dev, &p_uvc_dev_obj.uvc_devices_list, list_entry) {
        if (dev->index == index) {
            ESP_LOGI(TAG, "find uvc device");
            goto find_dev;
        }
    }

    return ESP_ERR_NOT_FOUND;

find_dev:
    if (if_open) {
        dev->require_frame_index = resolution_index;
        xEventGroupSetBits(dev->event_group, EVENT_START);
    } else {
        xEventGroupSetBits(dev->event_group, EVENT_STOP);
    }

    return ESP_OK;
}

esp_err_t app_uvc_get_dev_frame_info(int index, uvc_dev_info_t *dev_info)
{
  //  ESP_RETURN_ON_FALSE(index < UVC_CAM_NUM, ESP_ERR_INVALID_ARG, TAG, "Invalid index");
  //  ESP_RETURN_ON_FALSE(dev_info != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid pointer");

    uvc_dev_t *dev;
    int i = 0;
    SLIST_FOREACH(dev, &p_uvc_dev_obj.uvc_devices_list, list_entry) {
        if (i == index) {
            goto find_dev;
        } else {
            i++;
        }
    }

    return ESP_ERR_NOT_FOUND;

find_dev:
    dev_info->index  = dev->index;
    dev_info->if_streaming = dev->if_streaming;
    dev_info->active_resolution = dev->active_frame_index;
    
    // Limit resolution count to avoid buffer overflow
    dev_info->resolution_count = dev->frame_info_num;
    if (dev_info->resolution_count > MAX_RESOLUTION) {
        ESP_LOGW(TAG, "Device has %d resolutions, limiting to MAX_RESOLUTION (%d)", 
                 dev->frame_info_num, MAX_RESOLUTION);
        dev_info->resolution_count = MAX_RESOLUTION;
    }

    for (int i = 0; i < dev_info->resolution_count; i++) {
        dev_info->resolution[i].width = dev->frame_info[i].h_res;
        dev_info->resolution[i].height = dev->frame_info[i].v_res;
    }

    return ESP_OK;
}

esp_err_t app_uvc_get_connect_dev_num(int *num)
{
    ESP_RETURN_ON_FALSE(num != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid pointer");
    *num = p_uvc_dev_obj.uvc_dev_num;
    return ESP_OK;
}

uvc_host_frame_t *app_uvc_get_frame_by_index(int index)
{
    ESP_RETURN_ON_FALSE(index < UVC_CAM_NUM, NULL, TAG, "Invalid index");
    uvc_host_frame_t *frame = NULL;
    uvc_dev_t *dev;
    SLIST_FOREACH(dev, &p_uvc_dev_obj.uvc_devices_list, list_entry) {
        if (dev->index == index) {
            if (xQueueReceive(dev->frame_queue, &frame, pdMS_TO_TICKS(100)) == pdTRUE) {
                return frame;
            } else {
                return NULL;
            }
        }
    }
    return NULL;
}

esp_err_t app_uvc_return_frame_by_index(int index, uvc_host_frame_t *frame)
{
    ESP_RETURN_ON_FALSE(index < UVC_CAM_NUM, ESP_ERR_INVALID_ARG, TAG, "Invalid index");
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid frame pointer");

    uvc_dev_t *dev;
    SLIST_FOREACH(dev, &p_uvc_dev_obj.uvc_devices_list, list_entry) {
        if (dev->index == index) {
            return uvc_host_frame_return(dev->stream, frame);
        }
    }
    return ESP_ERR_NOT_FOUND;
}
