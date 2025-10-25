idf_component_register(SRCS "sensors_tasks.c" 
                       INCLUDE_DIRS "include" PRIV_REQUIRES "driver" "esp_adc"
                       REQUIRES "main" "lvgl" "nvs_flash" "sensors" 
                       )
