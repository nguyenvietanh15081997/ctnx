#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "stdbool.h"
#include "src/core/lv_obj.h"

#define EVENT_SYNC_SCREEN 0
#define SYNC_HOME_SCREEN 1
#define SYNC_MENU_SCREEN 2


extern volatile int32_t lv_encoder_diff;
extern volatile bool lv_button_clicked;
extern lv_obj_t *ui_Menu_Screen;
extern lv_obj_t *ui_Scan_Screen;
extern lv_obj_t *ui_stop_btn;

void setup_lvgl_indev(void);

void build_scan_ble_screen(void);
void build_main_menu_screen(void);
void build_dim_control_screen(void);
void build_cct_control_screen(void);
void build_rgb_control_screen(void);
void build_home_screen(void);
void build_logo_screen(void);

#ifdef __cplusplus
}
#endif

