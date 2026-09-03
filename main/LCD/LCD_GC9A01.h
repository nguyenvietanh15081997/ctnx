#ifndef LCD_GC9A01_H
#define LCD_GC9A01_H

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01 1
#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 0

void lcd_gc9a01_init(void);
#ifdef __cplusplus
}
#endif

#endif