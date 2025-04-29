#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "button.h"
#include "radio.h"

static const char LOG_TAG[] = __FILE__;

int button_click_down = 0;

void button_init()
{
    ESP_LOGI(LOG_TAG, "initialising button");
    pinMode(BUTTON_GPIO, INPUT_PULLDOWN);
    ESP_LOGI(LOG_TAG, "button initialised successfully");
}

void button_task_fun(void *_)
{
    while (true)
    {
        int button = digitalRead(BUTTON_GPIO);
        if (button == 0)
        {
            if (button_click_down > 0 && millis() - button_click_down > BUTTON_TASK_INTERVAL_MILLIS)
            {
                button_click_down = 0;
                ESP_LOGI(LOG_TAG, "button pressed down");
                radio_tx_start();
            }
        }
        else if (button_click_down == 0)
        {
            button_click_down = millis();
            ESP_LOGI(LOG_TAG, "button released");
            radio_tx_stop();
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(BUTTON_TASK_INTERVAL_MILLIS));
    }
}