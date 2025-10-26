#include "app_lcd.h"
#include "lv_demos.h"

#define TAG "app_lcd"


esp_lcd_panel_handle_t panel_handle = NULL;

SemaphoreHandle_t lvgl_mux = NULL;
static SemaphoreHandle_t touch_mux = NULL;
esp_lcd_touch_handle_t tp = NULL;
uint8_t state_touched =0;     

#define EXAMPLE_LVGL_TICK_PERIOD_MS    5    //2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 100   //500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 20    //1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (12 * 1024 / sizeof(StackType_t))  //(12 * 1024)  
#define EXAMPLE_LVGL_TASK_PRIORITY     1    //2

#define LCD_HOST    SPI2_HOST
#define TOUCH_HOST  I2C_NUM_0
#define LCD_BIT_PER_PIXEL              (16)
#define EXAMPLE_LCD_H_RES              410
#define EXAMPLE_LCD_V_RES              502

#define EXAMPLE_PIN_NUM_LCD_CS             (GPIO_NUM_21) 
#define EXAMPLE_PIN_NUM_LCD_PCLK           (GPIO_NUM_33)
#define EXAMPLE_PIN_NUM_LCD_DATA0          (GPIO_NUM_26)
#define EXAMPLE_PIN_NUM_LCD_DATA1          (GPIO_NUM_27)  
#define EXAMPLE_PIN_NUM_LCD_DATA2          (GPIO_NUM_29)
#define EXAMPLE_PIN_NUM_LCD_DATA3          (GPIO_NUM_31)
#define EXAMPLE_PIN_NUM_LCD_RST            (GPIO_NUM_30)
#define EXAMPLE_PIN_NUM_LCD_VCI_EN         (GPIO_NUM_28)
#define EXAMPLE_PIN_NUM_TOUCH_SCL          (GPIO_NUM_23)
#define EXAMPLE_PIN_NUM_TOUCH_SDA          (GPIO_NUM_36)
#define EXAMPLE_PIN_NUM_TOUCH_RST          (GPIO_NUM_34)
#define EXAMPLE_PIN_NUM_TOUCH_INT          (GPIO_NUM_22)



void init_lcd_lvgl(void *arg)
{
    // LCD_VCI_EN enable
    ESP_LOGI(TAG, "Enable LCD_VCI_EN");

    // ����GPIOΪ���ģʽ  
    gpio_config_t io_conf0;
    io_conf0.intr_type = GPIO_INTR_DISABLE;
    io_conf0.pin_bit_mask = (1ULL << EXAMPLE_PIN_NUM_LCD_VCI_EN);
    io_conf0.mode = GPIO_MODE_OUTPUT;
    io_conf0.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf0.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf0);
    // ��������Ϊ�ߵ�ƽ
    gpio_set_level(EXAMPLE_PIN_NUM_LCD_VCI_EN, 0);


    static lv_disp_draw_buf_t disp_buf; // ������Ϊ���ƻ��������ڲ�ͼ�λ�����
    static lv_disp_drv_t disp_drv;      //�����ص�����

    ESP_LOGI(TAG, "Install LCD-CO5300-SPI panel driver");
    const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                 EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8 + 8 );  //+8  EXAMPLE_LCD_V_RES

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO)); //SPI_DMA_CH_AUTO


    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, example_notify_lvgl_flush_ready, &disp_drv);

    co5300_vendor_config_t vendor_config = 
    {
        .flags = 
        {
            .use_qspi_interface = 1,
        },
    };

    // �� LCD ���ӵ� SPI ����
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    //esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = 
    {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 22, 0));
    
    // Mirror Y-axis to flip display upside-down
    //ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_LOGI(TAG, "LCD Y-axis mirrored (display flipped)");
    
    // �ڴ���Ļ�򱳹�֮ǰ���û����Խ�Ԥ�����ͼ��ˢ�µ���Ļ��
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    // ������Ļ����
    ESP_ERROR_CHECK(panel_co5300_set_brightness(panel_handle, 130)); // ��������Ϊ 15

    ESP_LOGI(TAG, "Install Touch-CST820-I2C panel driver");
    const i2c_config_t i2c_conf = 
    {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_param_config(TOUCH_HOST, &i2c_conf));
    //ESP_ERROR_CHECK(i2c_set_timeout(TOUCH_HOST, 0x02)); // 0x02: 2ms   //new
    ESP_ERROR_CHECK(i2c_driver_install(TOUCH_HOST, i2c_conf.mode, 0, 0, 0));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    
    //const esp_lcd_panel_io_i2c_config_t tp_io_config = TOUCH_IO_I2C_CONFIG();
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();

    // Attach the TOUCH to the I2C bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_HOST, &tp_io_config, &tp_io_handle));

    // touch_mux = xSemaphoreCreateBinary();
    // assert(touch_mux);

    const esp_lcd_touch_config_t tp_cfg = 
    {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = 
        {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = 
        {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        //.interrupt_callback = example_touch_isr_cb,
    };
    //ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(tp_io_handle, &tp_cfg, &tp));
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &tp));
    
    ESP_LOGI(TAG, "Install LVGL library");
    lv_init();

    // ���� LVGL ʹ�õĻ��ƻ�����
    // ����ѡ����ƻ������Ĵ�С����Ϊ��Ļ��С�� 1/10
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 15  * sizeof(lv_color_t),    MALLOC_CAP_DMA | MALLOC_CAP_8BIT   );  //   MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 15  * sizeof(lv_color_t),    MALLOC_CAP_DMA | MALLOC_CAP_8BIT );  //EXAMPLE_LCD_H_RES * 50  * sizeof(lv_color_t)
    assert(buf2); 
    // ��ʼ�� LVGL ���ƻ�����
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 15  ); //EXAMPLE_LCD_H_RES * 50

    ESP_LOGI(TAG, "Register LCD-Touch driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.rounder_cb = example_lvgl_rounder_cb;
    disp_drv.drv_update_cb = example_lvgl_update_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // LVGL �� Tick �ӿڣ�ʹ�� esp_timer ���� 2ms �������¼���
    const esp_timer_create_args_t lvgl_tick_timer_args = 
    {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    static lv_indev_drv_t indev_drv;    // �����豸�������򣨴�����
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;
    lv_indev_drv_register(&indev_drv);

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    
    ESP_LOGI(TAG, "Display UI panel");

    //���� LVGL API �����̰߳�ȫ�ģ��������������
    if (example_lvgl_lock(-1)) 
    {
        // Demo widgets disabled - camera feed will be displayed instead
        //lv_demo_widgets();      /* С����ʾ�� */
        //lv_demo_music();        /* ���������ֻ����ִ����ֲ�������ʾ */
        //lv_demo_stress();       /* LVGL ѹ������ */
        //lv_demo_benchmark();    /* ���ڲ��� LVGL ���ܻ�Ƚϲ�ͬ���õ���ʾ */

        // Set a simple background for camera display
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

        // Create two blue buttons for color testing
        lv_obj_t *btn1 = lv_btn_create(lv_scr_act());
        lv_obj_set_size(btn1, 100, 50);
        lv_obj_align(btn1, LV_ALIGN_TOP_LEFT, 20, 20);
        lv_obj_set_style_bg_color(btn1, lv_color_hex(0x0000FF), 0);  // Pure blue
        lv_obj_t *label1 = lv_label_create(btn1);
        lv_label_set_text(label1, "Blue1");
        lv_obj_center(label1);

        lv_obj_t *btn2 = lv_btn_create(lv_scr_act());
        lv_obj_set_size(btn2, 100, 50);
        lv_obj_align(btn2, LV_ALIGN_TOP_RIGHT, -20, 20);
        lv_obj_set_style_bg_color(btn2, lv_color_hex(0x0000FF), 0);  // Pure blue
        lv_obj_t *label2 = lv_label_create(btn2);
        lv_label_set_text(label2, "Blue2");
        lv_obj_center(label2);

        ESP_LOGI(TAG, "Created two blue test buttons");

        // �ͷŻ�����
        example_lvgl_unlock();
    }
}


static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    //ESP_LOGI("LVGL", "SPI flush done callback triggered");
    //lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready((lv_disp_drv_t *)user_ctx);
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // LVGL �涨���� [x1, x2] �������ˣ�������Ҫȷ�� x1 < x2+1
    if (offsetx1 > offsetx2) 
    {
        int temp = offsetx1;
        offsetx1 = offsetx2;
        offsetx2 = temp;
    }

    if (offsety1 > offsety2) 
    {
        int temp = offsety1;
        offsety1 = offsety2;
        offsety2 = temp;
    }

    // draw_bitmap ʹ�õ��Ƿǰ���ʽ���꣬�����Ҫ +1
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel_handle,offsetx1, offsety1,offsetx2 + 1, offsety2 + 1,color_map);

    if (err != ESP_OK) 
    {
        //ESP_LOGE("LVGL", "draw_bitmap failed: %s", esp_err_to_name(err));
        lv_disp_flush_ready(drv);  // һ��Ҫ���ã����ܳɹ����
    }
}

static void example_lvgl_update_cb(lv_disp_drv_t *drv)
{
    /* �� LVGL ����ת��Ļʱ����ת��ʾ�ʹ����� ���������������ʱ���� */
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) 
    {
    case LV_DISP_ROT_NONE:
        // ��תҺ����ʾ��
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        // ��תҺ������
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
        break;
    case LV_DISP_ROT_90:
        // ��תҺ����ʾ��
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        // ��תҺ������
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
        break;
    case LV_DISP_ROT_180:
        // ��תҺ����ʾ��
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        // ��תҺ������
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
        break;
    case LV_DISP_ROT_270:
        // ��תҺ����ʾ��
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        // ��תҺ������
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
        break;
    }
}

static void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;
    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of coordinate down to the nearest 2M number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    // round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;

}

static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;
    assert(tp);

    uint16_t tp_x;
    uint16_t tp_y;
    uint8_t tp_cnt = 0;
    // /* �������������е����ݶ�ȡ���ڴ��� */
    // if (xSemaphoreTake(touch_mux, 0) == pdTRUE) 
    // {
    //     esp_lcd_touch_read_data(tp);
    // }

     esp_lcd_touch_read_data(tp);

    /* �Ӵ�����������ȡ���� */
    bool tp_pressed = esp_lcd_touch_get_coordinates(tp, &tp_x, &tp_y, NULL, &tp_cnt, 1);

    /******************�����ٽ���*******************/
    //portENTER_CRITICAL(&mux_state_touched);   
    state_touched = tp_pressed;  //����״̬
    //portEXIT_CRITICAL(&mux_state_touched);    
    //if(tp_pressed) state_touched = 1;
    /******************�˳��ٽ���*******************/
   
    if (tp_pressed && tp_cnt > 0) 
    {
        data->point.x = tp_x;
        data->point.y = tp_y;
        data->state = LV_INDEV_STATE_PRESSED; 

        //ESP_LOGI(TAG, "Touch position: %d,%d", tp_x, tp_y);
    } else 
    {
        data->state = LV_INDEV_STATE_RELEASED;
        //state_touched = 0;
    }
}

static void example_increase_lvgl_tick(void *arg)
{
    /* ���� LVGL �Ѿ���ȥ�˶��ٺ���*/
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

/**********************************************************************************************************************************/
/**************************************************** example_lvgl_port_task Task **********************************************************/
/**********************************************************************************************************************************/
void example_lvgl_port_task(void *arg) 
{
    const char *LVGL_TASK_TAG = "lvgl_task";
    ESP_LOGI(LVGL_TASK_TAG, "Starting LVGL task!");
    ESP_LOGI("example_lvgl_port_task", "Running on core %d", xPortGetCoreID());

    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;

    /********************************���ڿ���ͼ����ʾ******************************************* */
    //lv_disp_load_scr(ui_ScreenLeBrewWelcome);

    for(int counter = 0;counter<5; counter++ )
    {
        if (example_lvgl_lock(-1)) 
        {
            lv_timer_handler(); 
            example_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    gpio_set_level(EXAMPLE_PIN_NUM_LCD_VCI_EN, 1);

    //�ֶ�����3.5s�Ļص��ö���������
    for(int counter = 0;counter<50; counter++ )
    {
        if (example_lvgl_lock(-1)) 
        {
            lv_timer_handler(); 
            example_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /********************************���ڼ��ض�����ʾ******************************************* */
    // lv_disp_load_scr(ui_ScreenLeBrewWelcome2);

    // for(int counter = 0;counter<50; counter++ )
    // {
    //     if (example_lvgl_lock(-1)) 
    //     {
    //         lv_timer_handler(); 
    //         example_lvgl_unlock();
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }


    //lv_disp_load_scr(ui_ScreenRoasting);

    while (1) 
    {
        if (example_lvgl_lock(-1)) 
        {
            task_delay_ms = lv_timer_handler(); 
            example_lvgl_unlock();
        } 

        // ���������С�ӳ�
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) 
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } 
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) 
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}


  

 