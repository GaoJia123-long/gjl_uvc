idf_component_register(
    SRCS "aviplay.c" "jpeg_encode.c" "jpeg2avi.c" "list.c"
    INCLUDE_DIRS "include"
    REQUIRES avi_player vfs fatfs esp_driver_jpeg
)
