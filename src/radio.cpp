#include <esp_log.h>
#include <esp_task_wdt.h>
#include <RadioLib.h>
#include "radio.h"

static const char LOG_TAG[] = __FILE__;

static SX1276 radio = new Module(RADIO_NSS_PIN, RADIO_DIO0_PIN, RADIO_RESET_PIN, RADIO_DIO1_PIN);
static SemaphoreHandle_t radio_mutex = xSemaphoreCreateMutex();

bool radio_tx = false;
float radio_centre_freq = RADIO_FIRST_CHAN + (RADIO_STEP_SIZE * (RADIO_MAX_STEPS / 2));
int sample_index = 0;
int radio_rssi[RADIO_MAX_STEPS][RADIO_RECENT_SAMPLES] = {0};

void radio_init()
{
    ESP_LOGI(LOG_TAG, "initialising radio");
    int state = radio.beginFSK(868.0, 0.5, 0.6, 125.0, 20, 16, true);
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to initialise radio: %d", state);
    }
    state = radio.setRxBandwidth(2.6);
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to set receiver bandwidth: %d", state);
    }
    state = radio.setAFCBandwidth(2.6);
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to set AFC bandwidth: %d", state);
    }
    state = radio.setAFC(true);
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to enable AFC: %d", state);
    }
    state = radio.setDataShapingOOK(1);
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to set OOK data shaping: %d", state);
    }
    ESP_LOGI(LOG_TAG, "radio initialised successfully");
}

void radio_lock()
{
    if (xSemaphoreTake(radio_mutex, RADIO_MUTEX_TIMEOUT_MILLIS) == pdFALSE)
    {
        ESP_LOGE(LOG_TAG, "failed to obtain radio lock");
        assert(false);
    }
}

void radio_unlock()
{
    xSemaphoreGive(radio_mutex);
}

void radio_tx_start()
{
    radio_lock();
    if (radio_tx)
    {
        radio_unlock();
        return;
    }
    radio_tx = true;
    int state = radio.setFrequency(radio_centre_freq);
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to set frequency: %d", state);
    }
    state = radio.transmitDirect();
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to transmit: %d", state);
    }
    ESP_LOGI(LOG_TAG, "radio transmission begins");
    radio_unlock();
}

void radio_tx_stop()
{
    radio_lock();
    if (!radio_tx)
    {
        radio_unlock();
        return;
    }
    int state = radio.standby();
    if (state != RADIOLIB_ERR_NONE)
    {
        ESP_LOGE(LOG_TAG, "failed to set radio to standby: %d", state);
    }
    radio_tx = false;
    radio_unlock();
}

void radio_scan()
{
    sample_index = (sample_index + 1) % RADIO_RECENT_SAMPLES;
    int start = millis();
    radio_lock();
    if (radio_tx)
    {
        radio_unlock();
        return;
    }
    for (int i = 0; i < RADIO_MAX_STEPS; i++)
    {
        float freq = radio_centre_freq + (RADIO_STEP_SIZE * (i - (RADIO_MAX_STEPS / 2)));
        int state = radio.setFrequency(freq);
        if (state != RADIOLIB_ERR_NONE)
        {
            ESP_LOGE(LOG_TAG, "failed to set frequency %.4f MHz: %d", freq, state);
        }
        state = radio.startReceive();
        if (state != RADIOLIB_ERR_NONE)
        {
            ESP_LOGE(LOG_TAG, "failed to start receive at %.4f MHz: %d", freq, state);
        }
        int rssi = (int)radio.getRSSI();
        radio_rssi[i][sample_index] = rssi;
    }
    radio_unlock();
    // ESP_LOGI(LOG_TAG, "radio scan completed in %d ms", millis() - start);
}

void radio_task_fun(void *_)
{
    while (true)
    {
        esp_task_wdt_reset();
        radio_scan();
        vTaskDelay(pdMS_TO_TICKS(RADIO_TASK_INTERVAL_MILLIS));
    }
}