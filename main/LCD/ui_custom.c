#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"

#include "ui.h"
#include "ui_custom.h"

#define MUTEX_LOCK(x) xSemaphoreTake((x), portMAX_DELAY)
#define MUTEX_UNLOCK(x) xSemaphoreGive((x))

ESP_EVENT_DEFINE_BASE(EVENT_BASE_COMMON);

SemaphoreHandle_t xGuiSemaphore;
volatile int32_t lv_encoder_diff = 0;
volatile bool lv_button_clicked = false;
// Group chứa các phần tử UI để encoder có thể điều khiển
lv_group_t *app_encoder_group = NULL;

int arc_value_to_cct(int16_t arc_value);

void OnEncoder(uint8_t direction)
{
    if (direction == 0)
    { // Xoay trái
        lv_encoder_diff--;
    }
    else if (direction == 1)
    { // Xoay phải
        lv_encoder_diff++;
    }
}

void app_encoder_clear_input(void)
{
    lv_button_clicked = false;
    lv_encoder_diff = 0;
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void encoder_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    // 1. Cập nhật độ lệch xoay (Scroll)
    data->enc_diff = lv_encoder_diff;
    lv_encoder_diff = 0; // Xóa đi sau khi LVGL đã đọc

    // 2. Cập nhật trạng thái nút bấm (Click)
    static bool last_state_was_pressed = false;

    if (lv_button_clicked)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        lv_button_clicked = false;
        last_state_was_pressed = true;
    }
    else if (last_state_was_pressed)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        last_state_was_pressed = false;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Lưu ý 2: Biến drv BẮT BUỘC phải là static hoặc toàn cục trong LVGL v8
static lv_indev_drv_t indev_drv;

// Hàm khởi tạo Input Device cho v8
void setup_lvgl_indev(void)
{
    xGuiSemaphore = xSemaphoreCreateMutex();

    // Kiểm tra xem việc cấp phát RAM cho Mutex có thành công không
    if (xGuiSemaphore == NULL)
    {
        ESP_LOGE("SYS", "Lỗi: Không đủ bộ nhớ để tạo xGuiSemaphore!");
        while (1)
        {
            // Dừng hệ thống nếu không tạo được Mutex (tránh crash ngầm)
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (app_encoder_group == NULL)
    {
        app_encoder_group = lv_group_create();
    }
    lv_group_set_default(app_encoder_group);
    // Khởi tạo struct drv bằng giá trị mặc định
    lv_indev_drv_init(&indev_drv);

    // Cài đặt loại và hàm callback
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read_cb;

    // Đăng ký driver với hệ thống LVGL
    lv_indev_t *encoder_indev = lv_indev_drv_register(&indev_drv);

    // Gán Group để phần cứng có thể điều khiển UI
    lv_indev_set_group(encoder_indev, app_encoder_group);
}

static void unregister_event_cb(void);
static void menu_btn_event_cb(lv_event_t *e) //{"Scan Devices", "WiFi Settings","Wifi Config","Reset GW", "Exit"};
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

        switch (index)
        {
        case 0:
            ESP_LOGI("UI", "Dim");
            build_dim_control_screen();
            break;
        case 1:
            ESP_LOGI("UI", "CCT");
            build_cct_control_screen();
            break;
        case 2:
            ESP_LOGI("UI", "RGB");
            build_rgb_control_screen();
            break;
        case 3:
            ESP_LOGI("UI", "Scan");
            build_scan_ble_screen();
            break;
        case 4:
        {
            ESP_LOGI("UI", "Chon: HOME");
            unregister_event_cb();
            build_home_screen();
            uint8_t screenIndex = SYNC_HOME_SCREEN;
            esp_event_post(EVENT_BASE_COMMON, EVENT_SYNC_SCREEN, &screenIndex, 1, pdMS_TO_TICKS(10));
            break;
        }
        }
    }
}

// =========================================================
// DANH SÁCH MENU
// =========================================================
static const char *menu_items[] = {
    "Dim",
    "CCT",
    "RGB",
    "Scan",
    LV_SYMBOL_HOME " Home",
    LV_SYMBOL_CLOSE " Exit"};

#define ITEM_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

static lv_obj_t *screen;
static lv_obj_t *cont;
static lv_obj_t *btn[ITEM_COUNT];

static void unregister_event_cb(void)
{
    for (uint32_t i = 0; i < ITEM_COUNT; i++)
    {
        if (btn[i] != NULL)
        {
            lv_obj_remove_event_cb(btn[i], menu_btn_event_cb);
        }
    }
}

void build_main_menu_screen(void)
{
    app_encoder_clear_input();
    vTaskDelay(pdMS_TO_TICKS(100)); // bỏ qua 100ms đầu tiên để tránh lỗi lv_clicked xuyên màn
    MUTEX_LOCK(xGuiSemaphore);
    // =========================================================
    // TẠO SCREEN MỚI
    // =========================================================
    if (!screen)
    {
        screen = lv_obj_create(NULL);
        // Background screen
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x17163D), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        // =========================================================
        // CONTAINER CHỨA MENU
        // =========================================================
        cont = lv_obj_create(screen);

        lv_obj_set_size(cont, 240, 240);

        // lv_obj_center(cont);
        lv_obj_set_align(cont, LV_ALIGN_CENTER);

        // Bật flex layout
        lv_obj_set_layout(cont, LV_LAYOUT_FLEX);

        // Xếp item theo chiều dọc
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        // Canh item giữa theo chiều ngang
        lv_obj_set_flex_align(cont,
                              LV_FLEX_ALIGN_START,  // LV_FLEX_ALIGN_START
                              LV_FLEX_ALIGN_CENTER, // LV_FLEX_ALIGN_CENTER
                              LV_FLEX_ALIGN_CENTER);

        // Khoảng cách giữa các item
        lv_obj_set_style_pad_row(cont, 15, 0); // 10

        // Padding container
        lv_obj_set_style_pad_all(cont, 20, 0); // 10

        // Scroll dọc
        lv_obj_set_scroll_dir(cont, LV_DIR_VER);
        // Tự động scroll để item được focus nằm giữa
        lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_CENTER);

        // Tắt scrollbar
        lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

        // Transparent container
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        // =========================================================
        // TẠO BUTTON MENU
        // =========================================================
        for (uint32_t i = 0; i < ITEM_COUNT; i++)
        {
            // -----------------------------------------------------
            // BUTTON
            // -----------------------------------------------------
            btn[i] = lv_btn_create(cont);
            lv_obj_set_size(btn[i], 160, 48); // lv_obj_set_width(btn, 160); lv_obj_set_height(btn, 54);

            // -----------------------------------------------------
            // STYLE DEFAULT
            // -----------------------------------------------------
            lv_obj_set_style_text_color(btn[i], lv_color_hex(0xF2EDED), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(btn[i], LV_RADIUS_CIRCLE, 0); // lv_obj_set_style_radius(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(btn[i], lv_color_hex(0x292856), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(btn[i], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(btn[i], 0, LV_PART_MAIN);

            lv_obj_set_style_shadow_width(btn[i], 10, 0);
            lv_obj_set_style_shadow_color(btn[i], lv_color_hex(0xA855F7), 0);
            lv_obj_set_style_shadow_opa(btn[i], LV_OPA_30, 0);
            // -----------------------------------------------------
            // LABEL
            // -----------------------------------------------------
            lv_obj_t *label = lv_label_create(btn[i]);
            lv_label_set_text(label, menu_items[i]);
            lv_obj_center(label);
            // lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            // Animation mượt
            lv_obj_set_style_anim_time(btn[i], 200, LV_PART_MAIN);

            // -----------------------------------------------------
            // STYLE FOCUSED
            // -----------------------------------------------------
            lv_obj_set_style_width(btn[i], 200, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_color(btn[i], lv_color_hex(0x37F4FA), LV_PART_MAIN | LV_STATE_FOCUSED);

            lv_obj_set_style_text_color(btn[i], lv_color_black(), LV_PART_MAIN | LV_STATE_FOCUSED);

            // -----------------------------------------------------
            // EVENT CLICK
            // -----------------------------------------------------
        }
    }
    else
    {
        // lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
        // xSemaphoreGive(xGuiSemaphore);
        // return;
    }
    // =========================================================
    // RESET ENCODER GROUP
    // =========================================================
    if (app_encoder_group != NULL)
    {
        lv_group_remove_all_objs(app_encoder_group);
    }
    unregister_event_cb();
    for (uint32_t i = 0; i < ITEM_COUNT; i++)
    {
        // -----------------------------------------------------
        // ENCODER GROUP
        // -----------------------------------------------------
        lv_group_add_obj(app_encoder_group, btn[i]);
        lv_obj_add_event_cb(btn[i], menu_btn_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }

    // =========================================================
    // FOCUS BUTTON ĐẦU TIÊN
    // =========================================================
    lv_obj_t *first_btn = lv_obj_get_child(cont, 0);

    if (first_btn != NULL)
    {
        lv_group_focus_obj(first_btn);
    }

    // Encoder ở mode navigate
    lv_group_set_editing(app_encoder_group, false);

    // =========================================================
    // LOAD SCREEN
    // =========================================================
    /*
    - true để xóa screen cũ sau khi animation kết thúc
    ~ lv_obj_del(old_screen);
      lv_scr_load(screen);
    */
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    // lv_obj_scroll_to_view(lv_obj_get_child(cont, 0), LV_ANIM_OFF);

    MUTEX_UNLOCK(xGuiSemaphore);
}

static void async_load_menu_cb(void *user_data)
{
    build_main_menu_screen();
}

static void ui_custom_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(e);
        lv_obj_t *scr = lv_scr_act();

        switch (key)
        {
        case LV_KEY_LEFT:
        {
            if (scr == ui_Dim_Screen)
            {
                int16_t dim = lv_arc_get_value(ui_dim_arc) - 10;
                if (dim <= 0)
                    dim = 0;
                // printf("event=%d, value: %d\n", lv_event_get_code(e), dim);
                lv_arc_set_value(ui_dim_arc, dim);
                char dim_text[8];
                sprintf(dim_text, "%d%%", dim);
                lv_label_set_text(ui_dim_label, dim_text);
            }
            else if (scr == ui_CCT_Screen)
            {
                int16_t cct = lv_arc_get_value(ui_cct_arc) - 10;
                if (cct <= 0)
                    cct = 0;
                // printf("event=%d, value: %d\n", lv_event_get_code(e), cct);
                lv_arc_set_value(ui_cct_arc, cct);
                char cct_text[8];
                sprintf(cct_text, "%dK", arc_value_to_cct(cct));
                lv_label_set_text(ui_cct_label, cct_text);
            }
            else if (scr == ui_RGB_Screen)
            {
                lv_color_hsv_t hsv = lv_colorwheel_get_hsv(ui_rgb_wheel);
                hsv.h += 36;
                if (hsv.h >= 360)
                    hsv.h = 0;
                lv_color_t color = lv_color_hsv_to_rgb(hsv.h, 100, 100); // H,S,V
                lv_obj_set_style_bg_color(ui_RGB_circle, color, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_colorwheel_set_hsv(ui_rgb_wheel, hsv);
            }
            break;
        }

        case LV_KEY_RIGHT:
        {
            if (scr == ui_Dim_Screen)
            {
                int16_t dim = lv_arc_get_value(ui_dim_arc) + 10;
                if (dim >= 100)
                    dim = 100;
                // printf("event=%d, value: %d\n", lv_event_get_code(e), dim);
                lv_arc_set_value(ui_dim_arc, dim);
                char dim_text[8];
                sprintf(dim_text, "%d%%", dim);
                lv_label_set_text(ui_dim_label, dim_text);
            }
            else if (scr == ui_CCT_Screen)
            {
                int16_t cct = lv_arc_get_value(ui_cct_arc) + 10;
                if (cct >= 100)
                    cct = 100;
                // printf("event=%d, value: %d\n", lv_event_get_code(e), cct);
                lv_arc_set_value(ui_cct_arc, cct);
                char cct_text[8];
                sprintf(cct_text, "%dK", arc_value_to_cct(cct));
                lv_label_set_text(ui_cct_label, cct_text);
            }
            else if (scr == ui_RGB_Screen)
            {
                lv_color_hsv_t hsv = lv_colorwheel_get_hsv(ui_rgb_wheel);
                hsv.h -= 18;
                if (hsv.h >= 360)
                    hsv.h = 359;
                lv_color_t color = lv_color_hsv_to_rgb(hsv.h, 100, 100); // H,S,V
                lv_obj_set_style_bg_color(ui_RGB_circle, color, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_colorwheel_set_hsv(ui_rgb_wheel, hsv);
            }
            break;
        }
        }
    }
    else if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        // printf("clicked \n");
        lv_obj_t *scr = lv_scr_act();
        if (scr == ui_Dim_Screen)
        {
            // lv_obj_remove_event_cb(ui_dim_label, ui_custom_cb);
        }
        else if (scr == ui_CCT_Screen)
        {
            // lv_obj_remove_event_cb(ui_cct_label, ui_custom_cb);
        }
        else if (scr == ui_RGB_Screen)
        {
            // lv_obj_remove_event_cb(ui_RGB_circle, ui_custom_cb);
        }
        else if (scr == ui_Scan_Screen)
        {
            // app_stop_scan_ble();
            lv_obj_remove_event_cb(ui_stop_btn, ui_custom_cb);
            lv_async_call(async_load_menu_cb, NULL);
        }
    }
}

void build_dim_control_screen(void)
{
    app_encoder_clear_input();
    MUTEX_LOCK(xGuiSemaphore);

    if (app_encoder_group != NULL)
    {
        lv_group_remove_all_objs(app_encoder_group);
        lv_group_add_obj(app_encoder_group, ui_dim_label);
    }
    int16_t dim = lv_arc_get_value(ui_dim_arc);
    char dim_text[8];
    sprintf(dim_text, "%d%%", dim);
    lv_label_set_text(ui_dim_label, dim_text);

    lv_group_focus_obj(ui_dim_label);
    lv_group_set_editing(app_encoder_group, true); // false: chỉ focus, ko điều khiển arc | true: điều khiển luôn arc
    static uint8_t firstBuildDimScreen = 1;
    if (firstBuildDimScreen)
    {
        firstBuildDimScreen = 0;
        lv_obj_add_event_cb(
            ui_dim_label,
            ui_custom_cb,
            LV_EVENT_KEY, // LV_EVENT_VALUE_CHANGED
            NULL);
    }

    // lv_obj_add_event_cb(ui_dim_label, ui_custom_cb, LV_EVENT_CLICKED, NULL);
    lv_disp_load_scr(ui_Dim_Screen);

    MUTEX_UNLOCK(xGuiSemaphore);
}

int arc_value_to_cct(int16_t arc_value)
{
    // arc (0-100) => CCT (800-6500K)
    return 2700 + arc_value * 38; // 38 = (6500 - 2700) / 100
}

void build_cct_control_screen(void)
{
    app_encoder_clear_input();
    MUTEX_LOCK(xGuiSemaphore);
    if (app_encoder_group != NULL)
    {
        lv_group_remove_all_objs(app_encoder_group);
        lv_group_add_obj(app_encoder_group, ui_cct_label);
    }
    int16_t cct = lv_arc_get_value(ui_cct_arc);
    char cct_text[8];
    sprintf(cct_text, "%dK", arc_value_to_cct(cct));
    lv_label_set_text(ui_cct_label, cct_text); // 2700- 6500K

    lv_group_focus_obj(ui_cct_label);
    lv_group_set_editing(app_encoder_group, true); // false: chỉ focus, ko điều khiển arc | true: điều khiển luôn arc
    static uint8_t firstBuildCctScreen = 1;
    if (firstBuildCctScreen)
    {
        firstBuildCctScreen = 0;
        lv_obj_add_event_cb(
            ui_cct_label,
            ui_custom_cb,
            LV_EVENT_KEY, // LV_EVENT_VALUE_CHANGED
            NULL);
    }
    // lv_obj_add_event_cb(ui_cct_label, ui_custom_cb, LV_EVENT_CLICKED, NULL);
    lv_disp_load_scr(ui_CCT_Screen);

    MUTEX_UNLOCK(xGuiSemaphore);
}

void build_rgb_control_screen(void)
{
    app_encoder_clear_input();
    MUTEX_LOCK(xGuiSemaphore);
    if (app_encoder_group != NULL)
    {
        lv_group_remove_all_objs(app_encoder_group);
        lv_group_add_obj(app_encoder_group, ui_RGB_circle);
    }
    lv_color_t color = lv_colorwheel_get_rgb(ui_rgb_wheel); // lv_color_hsv_to_rgb
    lv_obj_set_style_bg_color(ui_RGB_circle, color, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_group_focus_obj(ui_RGB_circle);
    lv_group_set_editing(app_encoder_group, true); // false: chỉ focus, ko điều khiển arc | true: điều khiển luôn arc
    static uint8_t firstBuildRGBScreen = 1;
    if (firstBuildRGBScreen)
    {
        firstBuildRGBScreen = 0;
        lv_obj_add_event_cb(
            ui_RGB_circle,
            ui_custom_cb,
            LV_EVENT_KEY, // LV_EVENT_VALUE_CHANGED
            NULL);
    }

    // lv_obj_add_event_cb(ui_RGB_circle, ui_custom_cb, LV_EVENT_CLICKED, NULL);
    lv_disp_load_scr(ui_RGB_Screen);

    MUTEX_UNLOCK(xGuiSemaphore);
}

/*
 *   HOME SCREEN
 */
static lv_obj_t *labelWeather;
static lv_obj_t *labelTime;
static lv_obj_t *labelDate;
static lv_obj_t *labelDay;
static lv_obj_t *iconWifi;
static lv_obj_t *iconWeather;
lv_obj_t *ui_HomeScreen;
static void async_load_home_cb(void *user_data)
{
    lv_disp_load_scr(ui_HomeScreen);
}
void build_home_screen(void)
{
    app_encoder_clear_input();
    MUTEX_LOCK(xGuiSemaphore);

    ui_HomeScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_HomeScreen, LV_OBJ_FLAG_SCROLLABLE);
    /* Background */
    lv_obj_set_style_bg_color(ui_HomeScreen, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HomeScreen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Weather */
    labelWeather = lv_label_create(ui_HomeScreen);
    lv_obj_set_width(labelWeather, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(labelWeather, LV_SIZE_CONTENT); /// 1
    lv_label_set_text(labelWeather, "28°C");
    lv_obj_set_style_text_color(labelWeather, lv_color_hex(0x3BCCA1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(labelWeather, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(labelWeather, LV_ALIGN_CENTER, 40, 65);

    /* Time */
    labelTime = lv_label_create(ui_HomeScreen);
    lv_obj_set_width(labelTime, LV_SIZE_CONTENT); /// 1
    lv_obj_set_height(labelTime, LV_SIZE_CONTENT);
    lv_label_set_text(labelTime, "20:08");
    lv_obj_set_style_text_color(labelTime, lv_color_hex(0x3BCCA1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_align(labelTime, LV_ALIGN_CENTER, 0, -15);

    /* Date */
    labelDate = lv_label_create(ui_HomeScreen);
    lv_label_set_text(labelDate, "Wed");
    lv_obj_set_style_text_color(labelDate, lv_color_hex(0x3BCCA1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(labelDate, LV_ALIGN_CENTER, 0, 13);

    /* Day */
    labelDay = lv_label_create(ui_HomeScreen);
    lv_label_set_text(labelDay, "29-08");
    lv_obj_set_style_text_color(labelDay, lv_color_hex(0x3BCCA1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(labelDay, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(labelDay, LV_ALIGN_CENTER, 45, 13);

    /* Distance */
    iconWifi = lv_label_create(ui_HomeScreen);
    lv_label_set_text(iconWifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(iconWifi, lv_color_hex(0x3BCCA1), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_text_color(iconWifi, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT); // mất wifi
    lv_obj_set_style_text_font(iconWifi, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(iconWifi, LV_ALIGN_CENTER, 0, -85);

    iconWeather = lv_img_create(ui_HomeScreen);
    lv_img_set_src(iconWeather, &ui_img_cloud2day_png);
    lv_obj_set_width(iconWeather, LV_SIZE_CONTENT);  /// 180
    lv_obj_set_height(iconWeather, LV_SIZE_CONTENT); /// 111
    lv_obj_align(iconWeather, LV_ALIGN_CENTER, -30, 65);
    lv_obj_add_flag(iconWeather, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(iconWeather, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    // mất wifi
    // static lv_point_t slash_points[] = {{0, 0}, {20, 20}};
    // lv_obj_t *iconWifiOff = lv_line_create(ui_HomeScreen);
    // lv_obj_set_size(iconWifiOff, 20, 20); // <-- thêm dòng này
    // lv_line_set_points(iconWifiOff, slash_points, 2);
    // lv_obj_set_style_line_width(iconWifiOff, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_line_color(iconWifiOff, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_line_rounded(iconWifiOff, true, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_align_to(iconWifiOff, iconWifi, LV_ALIGN_CENTER, -2, -2);

    // lv_disp_load_scr(ui_HomeScreen);
    lv_async_call(async_load_home_cb, NULL);
    MUTEX_UNLOCK(xGuiSemaphore);
}

void build_logo_screen(void)
{
    app_encoder_clear_input();
    MUTEX_LOCK(xGuiSemaphore);
    // meme_test();
    lv_disp_load_scr(ui_Ralli_Screen);
    MUTEX_UNLOCK(xGuiSemaphore);
}

/*============================================
*               Scan BLE mesh
=============================================*/

lv_obj_t *ui_Scan_Screen = NULL;
lv_obj_t *ui_scan_count_label;
lv_obj_t *ui_stop_btn;

// --- ANIMATION CHỚP TẮT SÓNG ---
static void anim_wave_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_arc_opa((lv_obj_t *)var, v, LV_PART_MAIN);
}

// --- HÀM TẠO 1 DẢI SÓNG VÀ KÍCH HOẠT HIỆU ỨNG ---
// r: đường kính, start/end: góc, delay: độ trễ chớp nháy
static lv_obj_t *create_radiating_wave(lv_obj_t *parent, int r, int start, int end, uint32_t delay)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, r, r);
    lv_obj_center(arc);

    // Chỉ lấy viền nền của Arc, giấu phần Indicator và Knob đi
    lv_arc_set_bg_angles(arc, start, end);
    lv_obj_remove_style(arc, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);

    // Style cho dải sóng
    lv_obj_set_style_arc_width(arc, 4, LV_PART_MAIN);                      // Độ dày của sóng
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00ffb2), LV_PART_MAIN); // Màu Cyan
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);                 // Bo tròn 2 đầu sóng
    lv_obj_set_style_arc_opa(arc, 0, LV_PART_MAIN);                        // Mặc định tàng hình
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    // Kích hoạt Animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_values(&a, 0, 255);                        // Sáng dần từ 0 lên 255
    lv_anim_set_time(&a, 600);                             // Thời gian sáng: 600ms
    lv_anim_set_playback_time(&a, 600);                    // Thời gian mờ đi: 600ms
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // Lặp vô hạn
    lv_anim_set_delay(&a, delay);                          // Độ trễ (Tạo cảm giác sóng lan tỏa)
    lv_anim_set_exec_cb(&a, anim_wave_opa_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    return arc;
}

void build_scan_ble_screen(void)
{
    app_encoder_clear_input();
    MUTEX_LOCK(xGuiSemaphore);
    if (!ui_Scan_Screen)
    {
        ui_Scan_Screen = lv_obj_create(NULL);
        lv_obj_clear_flag(ui_Scan_Screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(ui_Scan_Screen, lv_color_hex(0x17163D), LV_PART_MAIN | LV_STATE_DEFAULT);

        // Tiêu đề phía trên
        lv_obj_t *title = lv_label_create(ui_Scan_Screen);
        lv_label_set_text(title, "Searching Devices...");
        lv_obj_set_style_text_color(title, lv_color_hex(0x8D8C9A), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

        // ==========================================
        // 1. TẠO CÁC DẢI SÓNG TỎA RA 2 BÊN
        // LVGL quy ước: 0 độ ở bên phải, quay theo chiều kim đồng hồ.
        // ==========================================
        // SÓNG BÊN PHẢI (Góc từ 320 đến 40)
        create_radiating_wave(ui_Scan_Screen, 110, 330, 30, 0);   // Sóng trong cùng (Sáng trước)
        create_radiating_wave(ui_Scan_Screen, 150, 325, 35, 200); // Sóng giữa (Trễ 200ms)
        create_radiating_wave(ui_Scan_Screen, 190, 320, 40, 400); // Sóng ngoài cùng (Trễ 400ms)

        // SÓNG BÊN TRÁI (Góc từ 140 đến 220)
        create_radiating_wave(ui_Scan_Screen, 110, 150, 210, 0);
        create_radiating_wave(ui_Scan_Screen, 150, 145, 215, 200);
        create_radiating_wave(ui_Scan_Screen, 190, 140, 220, 400);

        // ==========================================
        // 2. LÕI TRUNG TÂM PHÁT SÁNG (Center Core)
        // ==========================================
        lv_obj_t *core = lv_obj_create(ui_Scan_Screen);
        lv_obj_set_size(core, 70, 70);
        lv_obj_center(core);
        lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(core, lv_color_hex(0x292856), LV_PART_MAIN);
        lv_obj_set_style_border_width(core, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(core, lv_color_hex(0x00ffb2), LV_PART_MAIN);

        // Hiệu ứng đổ bóng để làm lõi "phát sáng"
        lv_obj_set_style_shadow_color(core, lv_color_hex(0x00ffb2), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(core, 15, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(core, LV_OPA_60, LV_PART_MAIN);
        lv_obj_clear_flag(core, LV_OBJ_FLAG_SCROLLABLE);

        // Con số hiển thị thiết bị (Nằm gọn trong lõi)
        ui_scan_count_label = lv_label_create(core);
        lv_label_set_text(ui_scan_count_label, "0");
        lv_obj_set_style_text_color(ui_scan_count_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        // Nhớ thay font chữ to nếu bạn có, ví dụ:
        // lv_obj_set_style_text_font(ui_scan_count_label, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_center(ui_scan_count_label);

        // ==========================================
        // 3. NÚT STOP QUÉT SIÊU NGẦU (Dưới cùng)
        // ==========================================
        ui_stop_btn = lv_btn_create(ui_Scan_Screen);
        lv_obj_set_size(ui_stop_btn, 130, 40);
        lv_obj_align(ui_stop_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_radius(ui_stop_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);

        // Màu bình thường
        lv_obj_set_style_bg_color(ui_stop_btn, lv_color_hex(0x8C1A1A), LV_PART_MAIN | LV_STATE_DEFAULT);
        // Màu rực lên khi Encoder focus tới
        lv_obj_set_style_bg_color(ui_stop_btn, lv_color_hex(0xFC0707), LV_PART_MAIN | LV_STATE_FOCUSED);
        // Viền trắng nhẹ khi focus
        lv_obj_set_style_border_width(ui_stop_btn, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(ui_stop_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_t *btn_label = lv_label_create(ui_stop_btn);
        lv_label_set_text(btn_label, "Stop");
        lv_obj_center(btn_label);
    }
    lv_obj_add_event_cb(ui_stop_btn, ui_custom_cb, LV_EVENT_CLICKED, NULL);
    if (app_encoder_group != NULL)
    {
        lv_group_remove_all_objs(app_encoder_group);
        lv_group_add_obj(app_encoder_group, ui_stop_btn);
    }

    // Reset số đếm về 0 mỗi khi vào màn hình
    lv_label_set_text(ui_scan_count_label, "0");
    lv_disp_load_scr(ui_Scan_Screen);
    MUTEX_UNLOCK(xGuiSemaphore);
}