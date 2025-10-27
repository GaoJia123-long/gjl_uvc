
#include "uvc.h"
#include "sdmmc.h"
#include "jpeg2avi.h"

static const char *TAG = "uvc";

#define USB_DISCONNECT_PIN  GPIO_NUM_0


static lv_image_dsc_t img_dsc_main = {
    .header =
        {
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = CAM1_PIXEL_WIDTH,
            .h = CAM1_PIXEL_HEIGHT,
        },
    .data_size = CAM1_PIXEL_NUM*CAM1_PIXEL_CHANNEL ,
};

static lv_obj_t *ui_ImgMain;
static lv_obj_t * Maxtemp;
static lv_obj_t *videolabel;

static lv_obj_t * ui_Maxobj;
static lv_obj_t * ui_Mixobj;
static lv_obj_t * ui_centereobj;
static lv_obj_t * on_offbtn;
static lv_obj_t * on_displaybtn;

uint8_t pixel_array1[CAM1_PIXEL_NUM*CAM1_PIXEL_CHANNEL];

uint8_t diaplay_state = 0;


FILE *file_rgb_1080p;
static char filename[10];

uint8_t contrast_set[2] = {0x03, 0x05};
uint8_t para_set[2] = {0x03, 0x01};
uint8_t cail_set[2] = {0x03, 0x10};
uint8_t videcod_set[2] = {0x0A, 0x01};
uint8_t cailstr_set[2] = {0x03, 0x11};
uint8_t enhance_set[2] = {0x02, 0x05};
uint8_t cailstr_data[1] = {0x01};
uint8_t cail_data[27] = {0x01,0x32,0x00,0x00,0x00,0xB2,0x0C,0x00,0x00,0x61,
                          0x00,0x00,0x00,0x01,0x01,0x5A,0x05,0x00,0x00,0xF4,
                          0x01,0x00,0x00,0xF4,0x01,0x00,0x00};
uint8_t enhance_data[79] = {0x01,0x02,0x32,0x28,0x32,1,0x01,0x64,0x01,0x78,
                             0x05,0x00,0x00,0xe8,0x03,0x00,0x00,0x00,0x00,0x32,
                             0x00,0x32,0x01,0x01,0x00,0x32,0x00,0x00,0x00,0x00,
                             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                             0x00,0x01,0x02,0x0a,0x00,0x00,0x00,0x03,0x01,0x00,
                             0x08,0x07,0x00,0x00,0x40,0x06,0x00,0x00,0x01,0x00,
                             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,
                             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t para_data[80] = {0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x00,0xc8,0x00,
                          0x00,0x00,0xb0,0x04,0x00,0x00,0x61,0x00,0x00,0x00,
                          0x02,0x32,0x00,0x00,0x00,0x00,0xb0,0x04,0x00,0x00,
                          0x01,0x01,0x40,0x06,0x00,0x00,0x08,0x07,0x00,0x00,
                          0x64,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,
                          0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                          0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,
                          0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00};
uint8_t contrast_data[2] = {0x01, 0x08};

#if (CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_AUTO)
#define EXAMPLE_UVC_PROTOCOL_AUTO_COUNT     3
typedef struct {
    enum uvc_frame_format format;
    int width;
    int height;
    int fps;
    const char* name;
} uvc_stream_profile_t;

uvc_stream_profile_t uvc_stream_profiles[EXAMPLE_UVC_PROTOCOL_AUTO_COUNT] = {
    {UVC_FRAME_FORMAT_YUYV, 240, 240, 25, "240x240, fps 25"},
    {UVC_FRAME_FORMAT_MJPEG, 320, 240, 30, "320x240, fps 30"},
    {UVC_FRAME_FORMAT_MJPEG, 320, 240,  0, "320x240, any fps"}
};
#endif // CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_AUTO

#if (CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_CUSTOM)
#define FPS                 CONFIG_EXAMPLE_FPS_PARAM
#define WIDTH               CONFIG_EXAMPLE_WIDTH_PARAM
#define HEIGHT              CONFIG_EXAMPLE_HEIGHT_PARAM
#define FORMAT              CONFIG_EXAMPLE_FORMAT_PARAM
#endif // CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_CUSTOM

// Attached camera can be filtered out based on (non-zero value of) PID, VID, SERIAL_NUMBER
//#define PID 0X0102
//#define VID 0X2BDF

#define PID 0
#define VID 0

#define SERIAL_NUMBER NULL

#define UVC_CHECK(exp) do {                 \
    uvc_error_t _err_ = (exp);              \
    if(_err_ < 0) {                         \
        ESP_LOGE(TAG, "UVC error: %s",      \
                 uvc_error_string(_err_));  \
        assert(0);                          \
    }                                       \
} while(0)

static SemaphoreHandle_t ready_to_uninstall_usb;
static EventGroupHandle_t app_flags;
static lv_timer_t* s_player_timer = NULL;

// Handles common USB host library events
static void usb_lib_handler_task(void *args)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
        // Give ready_to_uninstall_usb semaphore to indicate that USB Host library
        // can be deinitialized, and terminate this task.
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            xSemaphoreGive(ready_to_uninstall_usb);
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t initialize_usb_host_lib(void)
{
    TaskHandle_t task_handle = NULL;

    const usb_host_config_t host_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        return err;
    }

    ready_to_uninstall_usb = xSemaphoreCreateBinary();
    if (ready_to_uninstall_usb == NULL) {
        usb_host_uninstall();
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(usb_lib_handler_task, "usb_events", 4096, NULL, 2, &task_handle) != pdPASS) {
        vSemaphoreDelete(ready_to_uninstall_usb);
        usb_host_uninstall();
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void uninitialize_usb_host_lib(void)
{
    xSemaphoreTake(ready_to_uninstall_usb, portMAX_DELAY);
    vSemaphoreDelete(ready_to_uninstall_usb);

    if (usb_host_uninstall() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to uninstall usb_host");
    }
}

float envi_dataf = 0.0f;
float min_dataf = 0.0f;
float max_dataf = 0.0f;
float aver_dataf = 0.0f;
float center_dataf = 0.0f;
    uint8_t * data_p;
int  maxtep_x;
uint16_t  maxtep_y;
uint16_t  mintep_x;
uint16_t  mintep_y;

//const static char out[] = "/sdcard/out.txt";
/* This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
void frame_callback(uvc_frame_t *frame, void *ptr)
{
    static int sdflog = 0;
    static int i = 0;
    static size_t fps;
    static int64_t start_time;

    data_p = (uint8_t *)(frame->data);
    int64_t current_time = esp_timer_get_time();
    int envi_data = 0;
    int min_data = 0;
    int max_data = 0;
    int aver_data = 0;
    fps++;
    
    if (diaplay_state == 1) {
        return;
    }

    if (!start_time) {
        start_time = current_time;
    }
    

    if (current_time > start_time + 1000000) {
       ESP_LOGI(TAG, "yuv data_bytes: %d", frame->data_bytes);  
       ESP_LOGI(TAG, "yuv width: %d", frame->width);  
       ESP_LOGI(TAG, "yuv height: %d", frame->height);  
       ESP_LOGI(TAG, "yuv step: %d", frame->step);  
       ESP_LOGI(TAG, "yuv sequence: %d", frame->sequence);  
       ESP_LOGI(TAG, "yuv library_owns_data: %d", frame->library_owns_data);  
       ESP_LOGI(TAG, "yuv metadata_bytes: %d", frame->metadata_bytes);  
//
        ESP_LOGI(TAG, "fps: %u ", fps);
    //ESP_LOGI(TAG, "i: %d ", i);
    //    ESP_LOGI(TAG, "1:%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
    //                   data_p[0],data_p[1],data_p[2],data_p[3],data_p[4],data_p[5],data_p[6],data_p[7],data_p[8],data_p[9],
    //                   data_p[10],data_p[11],data_p[12],data_p[13],data_p[14],data_p[15],data_p[16],data_p[17],data_p[18],data_p[19],
    //                   data_p[20],data_p[21],data_p[22],data_p[23],data_p[24],data_p[25],data_p[26],data_p[27],data_p[28],data_p[29],
    //                   data_p[30],data_p[31],data_p[32],data_p[33],data_p[34],data_p[35],data_p[36],data_p[37],data_p[38],data_p[39],
    //                   data_p[40],data_p[41],data_p[42],data_p[43],data_p[44],data_p[45],data_p[46],data_p[47],data_p[48],data_p[49],
    //                   data_p[50],data_p[51],data_p[52],data_p[53],data_p[54],data_p[55],data_p[56],data_p[57],data_p[58],data_p[59],
    //                   data_p[60],data_p[61],data_p[62],data_p[63],data_p[64],data_p[65],data_p[66],data_p[67],data_p[68],data_p[69],
    //                   data_p[70],data_p[71],data_p[72],data_p[73],data_p[74],data_p[75],data_p[76],data_p[77],data_p[78],data_p[79],
    //                   data_p[80],data_p[81],data_p[82],data_p[83],data_p[84],data_p[85],data_p[86],data_p[87],data_p[88],data_p[89],
    //                   data_p[90],data_p[91],data_p[92],data_p[93],data_p[94],data_p[95],data_p[96],data_p[97],data_p[98],data_p[99]); 
    //    ESP_LOGI(TAG, "2:%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
    //                   data_p[0+100],data_p[1+100],data_p[2+100],data_p[3+100],data_p[4+100],data_p[5+100],data_p[6+100],data_p[7+100],data_p[8+100],data_p[9+100],
    //                   data_p[10+100],data_p[11+100],data_p[12+100],data_p[13+100],data_p[14+100],data_p[15+100],data_p[16+100],data_p[17+100],data_p[18+100],data_p[19+100],
    //                   data_p[20+100],data_p[21+100],data_p[22+100],data_p[23+100],data_p[24+100],data_p[25+100],data_p[26+100],data_p[27+100],data_p[28+100],data_p[29+100],
    //                   data_p[30+100],data_p[31+100],data_p[32+100],data_p[33+100],data_p[34+100],data_p[35+100],data_p[36+100],data_p[37+100],data_p[38+100],data_p[39+100],
    //                   data_p[40+100],data_p[41+100],data_p[42+100],data_p[43+100],data_p[44+100],data_p[45+100],data_p[46+100],data_p[47+100],data_p[48+100],data_p[49+100],
    //                   data_p[50+100],data_p[51+100],data_p[52+100],data_p[53+100],data_p[54+100],data_p[55+100],data_p[56+100],data_p[57+100],data_p[58+100],data_p[59+100],
    //                   data_p[60+100],data_p[61+100],data_p[62+100],data_p[63+100],data_p[64+100],data_p[65+100],data_p[66+100],data_p[67+100],data_p[68+100],data_p[69+100],
    //                   data_p[70+100],data_p[71+100],data_p[72+100],data_p[73+100],data_p[74+100],data_p[75+100],data_p[76+100],data_p[77+100],data_p[78+100],data_p[79+100],
    //                   data_p[80+100],data_p[81+100],data_p[82+100],data_p[83+100],data_p[84+100],data_p[85+100],data_p[86+100],data_p[87+100],data_p[88+100],data_p[89+100],
    //                   data_p[90+100],data_p[91+100],data_p[92+100],data_p[93+100],data_p[94+100],data_p[95+100],data_p[96+100],data_p[97+100],data_p[98+100],data_p[99+100]); 
    //    ESP_LOGI(TAG, "3:%02x-%02x-%02x-%02x",data_p[5367],data_p[5368],data_p[5369],data_p[5370]);

#if 0
   // if(current_frame_index < buffer_frames){
   // memcpy(frame_buffer + (current_frame_index * frame_size), 
   //    pixel_array1, 
   //    frame_size);
   //     current_frame_index = current_frame_index + 1;   
   //     ESP_LOGE(TAG, "Frame buffer overflow, current_frame_index: %d", current_frame_index); 
   // }
   // else{
        if(sdflog < 10){
        //    char * data_d = frame->data;
        sprintf(filename, "/sdcard/%d.rgb", sdflog + 1);
        file_rgb_1080p = fopen(filename, "wb");
        ESP_LOGI(TAG, "raw_file_1080:%s", filename);
        if (file_rgb_1080p == NULL) {
            ESP_LOGE(TAG, "fopen file_rgb_1080p error");
            sdflog ++;
            return;
        }
///
        fwrite(pixel_array1, sizeof(uint8_t),115200, file_rgb_1080p);
        fclose(file_rgb_1080p);
////
////
        sdflog ++;
        }
    //}
#endif

        start_time = current_time;
        fps = 0;
    }
    envi_data = data_p[143]<<24 | data_p[142] <<16 | data_p[141] << 8 | data_p[140];
    memcpy(&envi_dataf, &envi_data, sizeof(float));

    min_data = data_p[147]<<24 | data_p[146] <<16 | data_p[145] << 8 | data_p[144];
    memcpy(&min_dataf, &min_data, sizeof(float));

    max_data = data_p[151]<<24 | data_p[150] <<16 | data_p[149] << 8 | data_p[148];
    memcpy(&max_dataf, &max_data, sizeof(float));

    aver_data = data_p[155]<<24 | data_p[154] <<16 | data_p[153] << 8 | data_p[152];
    memcpy(&aver_dataf, &aver_data, sizeof(float));
//
    maxtep_x = 0.6 *(( data_p[159]<<24 | data_p[158] << 16 | data_p[157] << 8 | data_p[156] ) -600);
    maxtep_y = 0.6 *( data_p[163]<<24 | data_p[162] << 16 | data_p[161] << 8 | data_p[160] );

    mintep_x = 0.6 *(( data_p[167]<<24 | data_p[166] << 16 | data_p[165] << 8 | data_p[164]) -600) ;
    mintep_y = 0.6 *( data_p[171]<<24 | data_p[170] << 16 | data_p[169] << 8 | data_p[168]) ;
 //   ESP_LOGI(TAG, "maxtep_x:%d-%d", maxtep_x ,maxtep_y);

    center_dataf = ((float)(data_p[13761] << 8 | data_p[13760]))/64-50;
 //   ESP_LOGI(TAG, "center:%f-%d-%d",center_dataf,data_p[13761],data_p[13760]);

    yuv422_to_rgb565_uint8(&data_p[23072],pixel_array1,CAM1_PIXEL_WIDTH,CAM1_PIXEL_HEIGHT);

    // Stream received frame to client, if enabled
    //tcp_server_send(frame->data, frame->data_bytes);
}

void lv_player_timer(struct _lv_timer_t * t)
{
  //cvt_yuyv2rgb(data_p,pixel_array1,CAM1_PIXEL_WIDTH,CAM1_PIXEL_HEIGHT);

    if (diaplay_state == 1) {
        return;
    }
    //RGB888_Scale_Bilinear(pixel_array1, pixel_array2, 
    //                      CAM1_PIXEL_WIDTH, CAM1_PIXEL_HEIGHT,
    //                      CAM1_PIXEL_WIDTH_SCALE, CAM1_PIXEL_HEIGHT_SCALE);

    img_dsc_main.data = (const uint8_t *)pixel_array1;
    //ESP_LOGI(TAG, "timer 40ms");
    lv_image_set_src(ui_ImgMain, &img_dsc_main);
    lv_obj_align(ui_Maxobj, LV_ALIGN_TOP_LEFT, maxtep_x,maxtep_y);
    lv_obj_align(ui_Mixobj, LV_ALIGN_TOP_LEFT, mintep_x,mintep_y);
    lv_label_set_text_fmt(Maxtemp, "EnviTemp: %f\nMinTemp: %f\nMaxiTemp: %f\nAverTemp: %f\nCenterTemp: %f",envi_dataf,min_dataf,max_dataf,aver_dataf,center_dataf);

}


void button_callback(int button, int state, void *user_ptr)
    {
    printf("button %d state %d\n", button, state);
}

static void libuvc_adapter_cb(libuvc_adapter_event_t event)
{
    xEventGroupSetBits(app_flags, event);
}

static EventBits_t wait_for_event(EventBits_t event)
{
    return xEventGroupWaitBits(app_flags, event, pdTRUE, pdFALSE, portMAX_DELAY) & event;
}

static uvc_error_t uvc_negotiate_stream_profile(uvc_device_handle_t *devh,
                                                uvc_stream_ctrl_t *ctrl)
{
    uvc_error_t res;
#if (CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_AUTO)
  //  for (int idx = 0; idx < EXAMPLE_UVC_PROTOCOL_AUTO_COUNT; idx++) {
        int attempt = CONFIG_EXAMPLE_NEGOTIATION_ATTEMPTS;
        do {
            /*
            The uvc_get_stream_ctrl_format_size() function will attempt to set the desired format size.
            On first attempt, some cameras would reject the format, even if they support it.
            So we ask 3x by default. The second attempt is usually successful.
            */
                ESP_LOGI(TAG, "Negotiate streaming profile ddd ...");
            res = uvc_get_stream_ctrl_format_size(devh,
                                                  ctrl,
                                                  UVC_FRAME_FORMAT_YUYV,
                                                  CAM1_PIXEL_WIDTH,
                                                  CAM1_PIXEL_HEIGHT,
                                                  25);
        } while (--attempt && !(UVC_SUCCESS == res));
    //    if (UVC_SUCCESS == res) {
    //        break;
    //    }
  //  }

#elif (CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_CUSTOM)
    int attempt = CONFIG_EXAMPLE_NEGOTIATION_ATTEMPTS;
    while (attempt--) {
        ESP_LOGI(TAG, "Negotiate streaming profile %dx%d, %d fps ...", WIDTH, HEIGHT, FPS);
        res = uvc_get_stream_ctrl_format_size(devh, ctrl, FORMAT, WIDTH, HEIGHT, FPS);
        if (UVC_SUCCESS == res) {
            break;
        }
        ESP_LOGE(TAG, "Negotiation failed. Try again (%d) ...", attempt);
    }
#endif // CONFIG_EXAMPLE_UVC_PROTOCOL_MODE_CUSTOM

    if (UVC_SUCCESS == res) {
        ESP_LOGI(TAG, "Negotiation complete.");
    } else {
        ESP_LOGE(TAG, "Try another UVC USB device of change negotiation parameters.");
    }

    return res;
}

uvc_device_handle_t *devh;
uvc_stream_ctrl_t ctrl;
uint8_t video_state = 0; // 0: not streaming, 1: streaming

 TaskHandle_t avi_play_task_handle = NULL;

static void on_off(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED) {
    if(btn == on_offbtn){
        ESP_LOGI(TAG, "paraauto_but");
        if (video_state == 0){
            video_state = 1;
            lv_label_set_text_fmt(videolabel, "v:%d", video_state);
        }else{
            video_state = 0;
            lv_label_set_text_fmt(videolabel, "v:%d", video_state);
        }
    }
    if(btn == on_displaybtn){
        ESP_LOGI(TAG, "paraauto_but");
      if (avi_play_task_handle == NULL) {
                diaplay_state = 1;
                xTaskCreate((TaskFunction_t)avi_uconplay, "avi_play_task", 8192, NULL, 5, &avi_play_task_handle);
            }
    }
    //    uvc_stop_streaming(devh);
    //    uvc_start_streaming(devh, &ctrl, frame_callback, NULL, 0);
    }
}


int uvc_main(void)
{
    uvc_context_t *ctx;
    uvc_device_t *dev = NULL;

    uvc_error_t ret;
    app_flags = xEventGroupCreate();
    assert(app_flags);

    const gpio_config_t input_pin = {
        .pin_bit_mask = BIT64(USB_DISCONNECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_pin));

    ESP_ERROR_CHECK(initialize_usb_host_lib());

    libuvc_adapter_config_t config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = libuvc_adapter_cb
    };

    libuvc_adapter_set_config(&config);
    ESP_LOGI(TAG, "versiom 1.0");

    UVC_CHECK(uvc_init(&ctx, NULL));

    s_player_timer = lv_timer_create(lv_player_timer,40,NULL);  //启动播放定时器
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);
    ui_ImgMain = lv_image_create(lv_screen_active());
   // lv_obj_set_width(ui_ImgMain, CAM1_PIXEL_WIDTH_SCALE);
   // lv_obj_set_height(ui_ImgMain, CAM1_PIXEL_HEIGHT_SCALE);
    lv_obj_align(ui_ImgMain, LV_ALIGN_TOP_LEFT, 0, 0);

    ui_Maxobj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui_Maxobj, 10, 10);
    lv_obj_set_style_bg_color(ui_Maxobj, lv_color_hex(0xff0000), LV_PART_MAIN);
    lv_obj_align(ui_Maxobj, LV_ALIGN_TOP_LEFT,  0, 0);

    ui_Mixobj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui_Mixobj, 10, 10);
    lv_obj_set_style_bg_color(ui_Mixobj, lv_color_hex(0x00ff00), LV_PART_MAIN);
    lv_obj_align(ui_Mixobj, LV_ALIGN_TOP_LEFT,  0, 0);

    ui_centereobj = lv_obj_create(ui_ImgMain);
    lv_obj_set_size(ui_centereobj, 10, 10);
    lv_obj_align(ui_centereobj, LV_ALIGN_CENTER,  0, 0);

    Maxtemp = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(Maxtemp,&lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(Maxtemp, lv_color_hex(0xffffff), LV_PART_MAIN); 
    lv_obj_align(Maxtemp, LV_ALIGN_TOP_LEFT, 240, 0);

    on_offbtn = lv_button_create(lv_screen_active());     /*Add a button the current screen*/
    lv_obj_align(on_offbtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);                         /*Set its position*/
    lv_obj_set_size(on_offbtn, 50, 50);                          /*Set its size*/
    lv_obj_add_event_cb(on_offbtn, on_off, LV_EVENT_ALL, NULL);
    videolabel = lv_label_create(on_offbtn);
    lv_label_set_text_fmt(videolabel, "v:%d", video_state);

    on_displaybtn = lv_button_create(lv_screen_active());
    lv_obj_align(on_displaybtn, LV_ALIGN_BOTTOM_LEFT, 60, 0);
    lv_obj_set_size(on_displaybtn, 50, 50);
    
    lv_obj_add_event_cb(on_displaybtn, on_off, LV_EVENT_ALL, NULL);

   

   // lv_example_get_started_2();

    // Streaming takes place only when enabled in menuconfig
    //ESP_ERROR_CHECK(tcp_server_wait_for_connection());
    do {
        
        ESP_LOGI(TAG, "Waiting for USB UVC device connection ...");
        wait_for_event(UVC_DEVICE_CONNECTED);
        
        if (uvc_find_device(ctx, &dev, VID, PID, SERIAL_NUMBER) != UVC_SUCCESS) {
            ESP_LOGW(TAG, "UVC device not found");
            continue; // Continue waiting for UVC device
        }
        ESP_LOGI(TAG, "UVC device found");

        // UVC Device open
        ret = uvc_open(dev, &devh);
        if (ret == UVC_SUCCESS) {
            ESP_LOGI(TAG,"UVC Device open success: %d", ret);
        } else {
            ESP_LOGW(TAG,"Error UVC Device open: %d", ret);
        }      

        // Uncomment to print configuration descriptor
        //libuvc_adapter_print_descriptors(devh);

       // uvc_set_button_callback(devh, button_callback, NULL);

        // Print known device information
        uvc_print_diag(devh, stderr);
        // Negotiate stream profile
        vTaskDelay(pdMS_TO_TICKS(1000));  // 延时1ms（需启用宏pdMS_TO_TICKS）
        if (UVC_SUCCESS == uvc_negotiate_stream_profile(devh, &ctrl)) {
            // dwMaxPayloadTransferSize has to be overwritten to MPS (maximum packet size)
            // supported by ESP32-S2(S3), as libuvc selects the highest possible MPS by default.
            ctrl.dwMaxPayloadTransferSize = 6144;


            uvc_print_stream_ctrl(&ctrl, stderr);

            uer_get_len_data(devh,0x04,2);
            uer_get_cur_data(devh,0x04,4);
//
            uer_get_len_data(devh,0x05,2);
            uer_set_cur_data(devh,0x05,2,contrast_set);
            uer_get_len_data(devh,0x03,2);
            uer_set_cur_data(devh,0x03,2,contrast_data);
//
            
//
        vTaskDelay(pdMS_TO_TICKS(1000));
        //UVC_CHECK(uvc_start_streaming(devh, &ctrl, frame_callback, NULL, 0));
            ret = uvc_start_streaming(devh, &ctrl, frame_callback, NULL, 0);
            if (ret < UVC_SUCCESS) {
            ESP_LOGI(TAG,"uvc_start_streaming: %d", ret);
            uvc_stop_streaming(devh);
            uvc_start_streaming(devh, &ctrl, frame_callback, NULL, 0);
            } ;

            ESP_LOGI(TAG, "Streaming...");
        //    vTaskDelay(pdMS_TO_TICKS(10));  // 延时1ms（需启用宏pdMS_TO_TICKS）


            wait_for_event(UVC_DEVICE_DISCONNECTED);
           

            uvc_stop_streaming(devh);
            ESP_LOGI(TAG, "Done streaming.");
        } else {
            wait_for_event(UVC_DEVICE_DISCONNECTED);
        }
        // UVC Device close
        uvc_close(devh);
    } while (gpio_get_level(USB_DISCONNECT_PIN) != 0);

    //tcp_server_close_when_done();


    uvc_exit(ctx);
    ESP_LOGI(TAG, "UVC exited");

    uninitialize_usb_host_lib();

    return 0;
}



void yuv422_to_rgb565_uint8(uint8_t *yuyv, uint8_t *rgb, int width, int height) {
   for (int i = 0, j = 0; i < width * height * 2; i += 4, j += 4) {
        // 提取YUYV分量
        int y0 = yuyv[i];
        int u  = yuyv[i+1] - 128;
        int y1 = yuyv[i+2];
        int v  = yuyv[i+3] - 128;
        
        // YUV转RGB系数
        const int v_1436 = v * 1436;
        const int u_352 = u * 352;
        const int v_731 = v * 731;
        const int u_1814 = u * 1814;
        
        // 第一个像素转换
        int r = y0 + (v_1436 >> 10);
        int g = y0 - (u_352 >> 10) - (v_731 >> 10);
        int b = y0 + (u_1814 >> 10);
        
        // 裁剪并打包为小端RGB565
        uint16_t rgb0 = ((r < 0 ? 0 : (r > 255 ? 255 : r)) >> 3) << 11 |
                       ((g < 0 ? 0 : (g > 255 ? 255 : g)) >> 2) << 5 |
                       ((b < 0 ? 0 : (b > 255 ? 255 : b)) >> 3);
        
        // 第二个像素转换
        r = y1 + (v_1436 >> 10);
        g = y1 - (u_352 >> 10) - (v_731 >> 10);
        b = y1 + (u_1814 >> 10);
        
        uint16_t rgb1 = ((r < 0 ? 0 : (r > 255 ? 255 : r)) >> 3) << 11 |
                       ((g < 0 ? 0 : (g > 255 ? 255 : g)) >> 2) << 5 |
                       ((b < 0 ? 0 : (b > 255 ? 255 : b)) >> 3);
        
        // 存储为小端uint8数组（低位在前）
        rgb[j]   = (uint8_t)(rgb0 & 0xFF);  // 低字节
        rgb[j+1] = (uint8_t)(rgb0 >> 8);    // 高字节
        rgb[j+2] = (uint8_t)(rgb1 & 0xFF);
        rgb[j+3] = (uint8_t)(rgb1 >> 8);
    }
}
