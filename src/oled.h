#pragma once

#include <SSD1306Wire.h>

const int OLED_TASK_INTERVAL_MILLIS = (1000 / 20);

void oled_draw_string_line(int line_number, String text);

void oled_init();
void oled_display_refresh();
void oled_task_fun(void *_);