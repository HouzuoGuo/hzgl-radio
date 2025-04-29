#pragma once

const int RADIO_TASK_INTERVAL_MILLIS = 2;
const int RADIO_MUTEX_TIMEOUT_MILLIS = 1000;

const int RADIO_NSS_PIN = 18;
const int RADIO_DIO0_PIN = 26;
const int RADIO_RESET_PIN = 23;
const int RADIO_DIO1_PIN = 33;

const float RADIO_FIRST_CHAN = 868.0;
const float RADIO_STEP_SIZE = 0.20;
const int RADIO_MAX_STEPS = 25;
const int RADIO_RECENT_SAMPLES = 4;

extern bool radio_tx;
extern float radio_centre_freq;
extern int radio_rssi[RADIO_MAX_STEPS][RADIO_RECENT_SAMPLES];

void radio_init();
void radio_lock();
void radio_unlock();
void radio_tx_start();
void radio_tx_stop();
void radio_task_fun(void *);