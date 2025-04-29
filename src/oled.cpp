#include <esp_task_wdt.h>
#include "oled.h"
#include "radio.h"
#include "power.h"

static const char LOG_TAG[] = __FILE__;

static SSD1306Wire oled(OLED_I2C_ADDR, -1, -1, GEOMETRY_128_64, I2C_ONE, 400000);

void oled_init()
{
    ESP_LOGI(LOG_TAG, "initialising display");
    oled.init();
    oled.clear();
    oled.setBrightness(64);
    oled.setContrast(0xF1, 128, 0x40);
    oled.resetOrientation();
    oled.flipScreenVertically();
    oled.setTextAlignment(TEXT_ALIGN_LEFT);
    oled.setFont(ArialMT_Plain_10);
    oled.displayOn();
    ESP_LOGI(LOG_TAG, "display initialised successfully");
}

void oled_draw_string_line(int line_number, String text)
{
    oled.drawStringMaxWidth(0, line_number * OLED_FONT_HEIGHT_PX, 200, text);
}

void oled_display_refresh()
{
    char status_line[OLED_MAX_LINE_LEN + 1] = {0};
    if (radio_tx)
    {
        snprintf(status_line, OLED_MAX_LINE_LEN, "TRANSMITTING @ %.2f", radio_centre_freq);
    }
    else
    {
        snprintf(status_line, OLED_MAX_LINE_LEN, "Centre @ %.2fMHz", radio_centre_freq);
    }
    oled.clear();
    oled_draw_string_line(0, status_line);

    const int BAR_WIDTH = 6;
    const int BAR_MAX_HEIGHT = 50;
    const int BAR_BASE_Y = 63;
    const int CENTER_BAR_INDEX = RADIO_MAX_STEPS / 2;

    radio_lock();

    long sum_rssi_total = 0;
    int count = 0;
    for (int i = 0; i < RADIO_MAX_STEPS; i++)
    {
        for (int j = 0; j < RADIO_RECENT_SAMPLES; j++)
        {
            sum_rssi_total += radio_rssi[i][j];
            count++;
        }
    }
    int avg_rssi = (count > 0) ? (int)(sum_rssi_total / count) : -100;
    const int MARGIN_BELOW = 20;
    const int MARGIN_ABOVE = 10;
    const int ABS_MIN_RSSI = -100;
    const int ABS_MAX_RSSI = -70;
    int rssi_min = max(avg_rssi - MARGIN_BELOW, ABS_MIN_RSSI);
    int rssi_max = min(avg_rssi + MARGIN_ABOVE, ABS_MAX_RSSI);

    for (int i = 0; i < RADIO_MAX_STEPS; i++)
    {
        int x_start = i * BAR_WIDTH;

        long sum_rssi_channel = 0;
        for (int j = 0; j < RADIO_RECENT_SAMPLES; j++)
        {
            sum_rssi_channel += radio_rssi[i][j];
        }
        int rssi_avg = (int)(sum_rssi_channel / RADIO_RECENT_SAMPLES);

        int height = map(rssi_avg, rssi_min, rssi_max, 0, BAR_MAX_HEIGHT);
        height = constrain(height, 0, BAR_MAX_HEIGHT);
        int y_top = BAR_BASE_Y - height;

        for (int x = x_start; x < x_start + BAR_WIDTH - 1; x++)
        {
            oled.drawVerticalLine(x, y_top, height);
        }

        if (i == CENTER_BAR_INDEX)
        {
            int center_x = x_start + (BAR_WIDTH / 2) - 1;
            oled.drawVerticalLine(center_x, 14, 50);
        }
    }
    radio_unlock();
    oled.display();
}

void oled_task_fun(void *_)
{
    while (true)
    {
        esp_task_wdt_reset();
        oled_display_refresh();
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_INTERVAL_MILLIS));
    }
}
