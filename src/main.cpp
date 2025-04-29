// hzgl-radio firmware.

#include <Arduino.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <esp_task.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "supervisor.h"
#include "power.h"
#include "oled.h"
#include "button.h"
#include "radio.h"

static const char LOG_TAG[] = __FILE__;

void setup(void)
{
  // Monitor the arduino SDK main task and reboot automatically when it is stuck.
  ESP_ERROR_CHECK(esp_task_wdt_init(SUPERVISOR_WATCHDOG_TIMEOUT_SEC, true));
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

  esp_log_level_set("*", ESP_LOG_VERBOSE);
  Serial.begin(115200);
  ESP_LOGI(LOG_TAG, "hzgl-radio is starting up, xtal %d Mhz, cpu %d Mhz, apb %d Mhz",
           getXtalFrequencyMhz(), getCpuFrequencyMhz(), getApbFrequency() / 1000000);

  // Initialise hardware and peripherals in the correct order.
  power_init();
  oled_init();
  button_init();
  radio_init();
  // Start background tasks.
  supervisor_init();
  ESP_LOGI(LOG_TAG, "main task initialised successfully");
}

void loop()
{
  // Bluetooth and peripheral functions are in separate tasks created by the supervisor.
  // The arduino SDK main loop only maintains the watchdog.
  if (millis() < SUPERVISOR_UNCONDITIONAL_RESTART_MILLIS)
  {
    esp_task_wdt_reset();
  }
  else
  {
    ESP_LOGW(LOG_TAG, "performing a routine restart");
    esp_restart();
  }
  // Use arduino's delay instead of vTaskDelay to avoid a deadlock in arduino's loop function.
  delay(SUPERVISOR_TASK_LOOP_INTERVAL_MILLIS);
}
