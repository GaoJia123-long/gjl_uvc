file(GLOB USER_components "components/*.c")

idf_component_register(SRCS "app_lcd.c" "app_main.c" "app_uvc.c" "app_https.c" "app_ble.c" "app_wifi.c" "gatt_svr.c" 
 		${USER_components}
		"${IDF_PATH}/components/esp_lcd/rgb/include"
  		"../components/esp_lcd_co5300/include"
   		"../components/esp_lcd_touch_cst9217/include"
		# "../components/sensors/include"
		# "../components/algorithms/include"
		# "../components/tasks/include"
		# PRIV_REQUIRES esp_driver_jpeg 
        INCLUDE_DIRS "."
		# REQUIRES "lvgl" "esp_lcd" "esp_lcd_co5300" "esp_lcd_touch_cst9217" "esp_lcd_touch"
		)
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)


