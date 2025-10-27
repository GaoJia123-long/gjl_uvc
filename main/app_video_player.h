#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Show video file list page
 * @param parent Parent screen to return to
 */
void video_player_show_file_list(lv_obj_t *parent);

/**
 * @brief Play selected video file
 * @param filename Full path to AVI file
 * @param parent Parent screen to return to
 */
void video_player_play_video(const char *filename, lv_obj_t *parent);

/**
 * @brief Close video player and return to parent screen
 */
void video_player_close(void);

#ifdef __cplusplus
}
#endif


