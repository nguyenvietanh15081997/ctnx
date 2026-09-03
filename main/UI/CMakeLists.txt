SET(SOURCES components/ui_comp_dim_arc.c
    components/ui_comp.c
    screens/ui_Ralli_Screen.c
    screens/ui_Dim_Screen.c
    screens/ui_CCT_Screen.c
    screens/ui_RGB_Screen.c
    screens/ui_Clock_Screen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_logoralli_png.c
    images/ui_img_cloud2rain3day_png.c
    images/ui_img_clearcloudday_png.c
    images/ui_img_clearcloudnight_png.c
    images/ui_img_cloud2day_png.c
    images/ui_img_cloud2night_png.c
    images/ui_img_cloud2rain3night_png.c)

add_library(ui ${SOURCES})
