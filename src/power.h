#pragma once

#ifdef AXP192
#define XPOWERS_CHIP_AXP192 = 1
#endif

#ifdef AXP2101
#define XPOWERS_CHIP_AXP2101 = 1
#endif

#include <XPowersLib.h>

// POWER_PMU_IRQ is the IRQ of the AXP192 and AXP2101 PMU chip on TTGO-TBeam.
#define POWER_PMU_IRQ 35

const int POWER_MUTEX_TIMEOUT_MILLIS = 10;
const int POWER_TASK_INTERVAL_MILLIS = 1000;
const int POWER_LOG_STATUS_INTERVAL_MILLIS = POWER_TASK_INTERVAL_MILLIS * 60;
const int POWER_STATUS_INTERVAL_MILLIS = POWER_TASK_INTERVAL_MILLIS * 3;

struct power_status
{
    int batt_millivolt, usb_millivolt;
    bool is_batt_charging, is_usb_power_available;
    // batt_milliamp is the rate of milliamps entering(+) in or discharged(-) from the battery.
    float batt_milliamp;
    // power_draw_milliamp is the rate of milliamps drawn from a power source. The number is always positive.
    float power_draw_milliamp;
};

void power_init();
void power_i2c_lock();
void power_i2c_unlock();
int power_get_uptime_sec();
void power_read_status();
void power_log_status();
void power_task_fun(void *);
void power_set_pmu_irq_flag(void);