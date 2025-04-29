#include <esp_log.h>
#include <esp_task_wdt.h>
#include <SPI.h>
#include "oled.h"
#include "power.h"

static const char LOG_TAG[] = __FILE__;

static XPowersPMU *pmu;
static struct power_status status;
static SemaphoreHandle_t i2c_mutex = xSemaphoreCreateMutex();
static bool pmu_irq_flag = false;

void power_init()
{
    ESP_LOGI(LOG_TAG, "initialising power and peripherals");
    power_i2c_lock();

    if (!Wire.begin(I2C_SDA, I2C_SCL, 400000))
    {
        ESP_LOGE(LOG_TAG, "failed to initialise I2C");
    }

#ifdef AXP192
    pmu = new XPowersAXP192(Wire);
    // Both AXP192 and AXP2101 use the same I2C address - 0x34.
    if (!pmu->begin(Wire, AXP192_SLAVE_ADDRESS, I2C_SDA, I2C_SCL))
    {
        ESP_LOGE(LOG_TAG, "failed to initialise AXP power management chip");
    }
#endif
#ifdef AXP2101
    pmu = new XPowersAXP2101(Wire);
    // Both AXP192 and AXP2101 use the same I2C address - 0x34.
    if (!pmu->begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL))
    {
        ESP_LOGE(LOG_TAG, "failed to initialise AXP power management chip");
    }
#endif

    // Set USB power limits.
    if (pmu->getChipModel() == XPOWERS_AXP192)
    {
        ESP_LOGI(LOG_TAG, "setting up AXP192");
#ifdef AXP192
        // https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/AXP192_datasheet_en.pdf & AXP192%20Datasheet%20v1.13_cn..pdf
        pmu->setVbusVoltageLimit(XPOWERS_AXP192_VBUS_VOL_LIM_4V1);
        // There is no suitable current limit for 1.5A, the closest is only 0.5A.
        pmu->setVbusCurrentLimit(XPOWERS_AXP192_VBUS_CUR_LIM_OFF);
        // "wide input voltage range": 2.9V~6.3V
        // When using 3V for the shutdown voltage, the PMU actually shuts down at ~3.4V.
        pmu->setSysPowerDownVoltage(2800);

        pmu->enablePowerKeyLongPressPowerOff();
        pmu->setPowerKeyPressOffTime(XPOWERS_AXP192_POWEROFF_4S);
        pmu->setPowerKeyPressOnTime(XPOWERS_POWERON_2S);

        pmu->disableTSPinMeasure();
        pmu->setChargingLedMode(false);

        pmu->enableBattDetection();
        pmu->enableVbusVoltageMeasure();
        pmu->enableBattVoltageMeasure();
        pmu->enableSystemVoltageMeasure();

        // https://www.solomon-systech.com/product/ssd1306/
        // "– VDD= 1.65V – 3.3V, <VBAT for IC Logic"
        // "– VBAT= 3.3V – 4.2V for charge pump regulator circuit"
        pmu->setDC1Voltage(3300);
        pmu->enableDC1();
        // DC2 is unused.
        pmu->disableDC2();
        // https://cdn-shop.adafruit.com/product-files/3179/sx1276_77_78_79.pdf
        // "Supply voltage = 3.3 V"
        pmu->setLDO2Voltage(3300);
        pmu->enableLDO2();
        // https://www.u-blox.com/sites/default/files/products/documents/NEO-6_DataSheet_(GPS.G6-HW-09005).pdf
        // "NEO-6Q/M NEO-6P/V/T Min: 2.7, Typ: 3.0, Max: 3.6"
        pmu->setLDO3Voltage(3000);

        // Start charging the battery if it is installed.
        // The maximum supported charging current is 1.4A.
        pmu->setChargerConstantCurr(XPOWERS_AXP192_CHG_CUR_1000MA);
        pmu->setChargerTerminationCurr(XPOWERS_AXP192_CHG_ITERM_LESS_10_PERCENT);
        pmu->setChargeTargetVoltage(XPOWERS_AXP192_CHG_VOL_4V2);

        // Start charging the GPS memory backup battery.
        pmu->setBackupBattChargerVoltage(XPOWERS_AXP192_BACKUP_BAT_VOL_3V1);
        pmu->setBackupBattChargerCurr(XPOWERS_AXP192_BACKUP_BAT_CUR_100UA);

        // Conserve power by disabling temperature measurement.
        pmu->disableTemperatureMeasure();

        // Handle power management events.
        pinMode(POWER_PMU_IRQ, INPUT);
        attachInterrupt(POWER_PMU_IRQ, power_set_pmu_irq_flag, FALLING);
        pmu->disableIRQ(XPOWERS_AXP192_ALL_IRQ);
        pmu->clearIrqStatus();
        pmu->enableIRQ(
            XPOWERS_AXP192_BAT_INSERT_IRQ | XPOWERS_AXP192_BAT_REMOVE_IRQ |
            XPOWERS_AXP192_VBUS_INSERT_IRQ | XPOWERS_AXP192_VBUS_REMOVE_IRQ |
            XPOWERS_AXP192_PKEY_SHORT_IRQ |
            XPOWERS_AXP192_BAT_CHG_DONE_IRQ | XPOWERS_AXP192_BAT_CHG_START_IRQ);
        pmu->clearIrqStatus();
#endif
    }
    else if (pmu->getChipModel() == XPOWERS_AXP2101)
    {
        ESP_LOGI(LOG_TAG, "setting up AXP2101");
#ifdef AXP2101
        // https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/AXP2101_Datasheet_V1.0_en.pdf
        // VBUS min 3.9V, max 5.5V.
        pmu->setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V12);
        // VBUS max 2A both in and out.
        pmu->setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
        // VBAT operating range is between 2.5V and 4.5V. The PMU shuts down just below 3.3V.
        // When using 3V for the shutdown voltage and 5% for the threshold, the PMU actually shuts down at ~3.1v.
        pmu->setSysPowerDownVoltage(3100);
        pmu->setLowBatShutdownThreshold(7);

        pmu->setLongPressPowerOFF();
        pmu->setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
        pmu->setPowerKeyPressOnTime(XPOWERS_POWERON_2S);

        pmu->disableTSPinMeasure();
        pmu->setChargingLedMode(false);
        pmu->disableDC2();
        pmu->disableDC4();
        pmu->disableDC5();
        pmu->disableALDO1();
        pmu->disableALDO4();
        pmu->disableBLDO1();
        pmu->disableBLDO2();
        pmu->disableDLDO1();
        pmu->disableDLDO2();

        pmu->enableBattDetection();
        pmu->enableVbusVoltageMeasure();
        pmu->enableBattVoltageMeasure();
        pmu->enableSystemVoltageMeasure();

        // https://www.solomon-systech.com/product/ssd1306/
        // "– VDD= 1.65V – 3.3V, <VBAT for IC Logic"
        // "– VBAT= 3.3V – 4.2V for charge pump regulator circuit"
        pmu->setDC1Voltage(3300);
        pmu->enableDC1();
        // DC2 is unused.
        pmu->disableDC2();
        // https://cdn-shop.adafruit.com/product-files/3179/sx1276_77_78_79.pdf
        // "Supply voltage = 3.3 V"
        pmu->setALDO2Voltage(3300);
        pmu->enableALDO2();
        // https://www.u-blox.com/sites/default/files/products/documents/NEO-6_DataSheet_(GPS.G6-HW-09005).pdf
        // "NEO-6Q/M NEO-6P/V/T Min: 2.7, Typ: 3.0, Max: 3.6"
        pmu->setALDO3Voltage(3000);
        pmu->enableALDO3();

        // Conserve power by disabling temperature measurement.
        pmu->disableTemperatureMeasure();

        // Start charging the battery if it is installed.
        pmu->setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_100MA);
        pmu->setChargerConstantCurr(XPOWERS_AXP202_CHG_CUR_1000MA);
        pmu->setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_50MA);
        pmu->setChargeTargetVoltage(XPOWERS_AXP202_CHG_VOL_4V2);

        // Start charging the GPS memory backup battery.
        pmu->setButtonBatteryChargeVoltage(3100);
        pmu->enableButtonBatteryCharge();

        // Handle power management events.
        pinMode(POWER_PMU_IRQ, INPUT);
        attachInterrupt(POWER_PMU_IRQ, power_set_pmu_irq_flag, FALLING);
        pmu->disableIRQ(XPOWERS_AXP192_ALL_IRQ);
        pmu->clearIrqStatus();
        pmu->enableIRQ(
            XPOWERS_AXP202_BAT_INSERT_IRQ | XPOWERS_AXP202_BAT_REMOVE_IRQ |
            XPOWERS_AXP202_VBUS_INSERT_IRQ | XPOWERS_AXP202_VBUS_REMOVE_IRQ |
            XPOWERS_AXP202_PKEY_SHORT_IRQ |
            XPOWERS_AXP202_BAT_CHG_DONE_IRQ | XPOWERS_AXP202_BAT_CHG_START_IRQ);
        pmu->clearIrqStatus();
#endif
    }
    power_i2c_unlock();
    ESP_LOGI(LOG_TAG, "power and peripherals initialised successfully");
}

void power_set_pmu_irq_flag(void)
{
    pmu_irq_flag = true;
}

void power_i2c_lock()
{
    if (xSemaphoreTake(i2c_mutex, POWER_MUTEX_TIMEOUT_MILLIS) == pdFALSE)
    {
        ESP_LOGE(LOG_TAG, "failed to obtain i2c_mutex lock");
        assert(false);
    }
}

void power_i2c_unlock()
{
    xSemaphoreGive(i2c_mutex);
}

void power_read_handle_lastest_irq()
{
    power_i2c_lock();
    if (pmu_irq_flag)
    {
        pmu_irq_flag = false;
        pmu->getIrqStatus();
        if (pmu->isBatInsertIrq())
        {
            ESP_LOGI(LOG_TAG, "battery inserted");
        }
        if (pmu->isBatRemoveIrq())
        {
            ESP_LOGI(LOG_TAG, "battery removed");
        }
        if (pmu->isBatChargeDoneIrq())
        {
            ESP_LOGI(LOG_TAG, "battery charging completed");
        }
        if (pmu->isPekeyShortPressIrq())
        {
            ESP_LOGI(LOG_TAG, "Pekey short click");
        }
        if (pmu->isPekeyLongPressIrq())
        {
            ESP_LOGW(LOG_TAG, "shutting down");
            pmu->setChargingLedMode(false);
            pmu->shutdown();
        }
        pmu->clearIrqStatus();
    }
    power_i2c_unlock();
}

int power_get_uptime_sec()
{
    return (esp_timer_get_time() / 1000000);
}

void power_read_status()
{
    power_i2c_lock();
    status.is_batt_charging = pmu->isCharging();
    status.batt_millivolt = pmu->getBattVoltage();
    if (status.batt_millivolt < 500)
    {
        // The AXP chip occasionally produces erranous and exceedingly low battery voltage readings even without a battery installed.
        status.batt_millivolt = 0;
    }
    status.usb_millivolt = pmu->getVbusVoltage();
#ifdef AXP192
    if (status.is_batt_charging)
    {
        status.batt_milliamp = pmu->getBatteryChargeCurrent();
    }
    else
    {
        status.batt_milliamp = -pmu->getBattDischargeCurrent();
    }
    status.power_draw_milliamp = pmu->getVbusCurrent();
#endif
#ifdef AXP2101
    // Unsure if the library is capable of reading the current consumptino: https://github.com/lewisxhe/XPowersLib/issues/12
    // Just default to typical readings.
    status.power_draw_milliamp = 80;
    status.batt_milliamp = 0;
    if (status.batt_millivolt > 2000 && !status.is_batt_charging && status.usb_millivolt < 4000)
    {
        status.batt_milliamp = -80;
    }
#endif
    // The power management chip always draws power from USB when it is available.
    // Use battery discharging current as a condition too because the VBus current occasionally reads 0.
    status.is_usb_power_available = status.is_batt_charging || status.batt_milliamp > 3 || status.batt_millivolt < 3000 || status.power_draw_milliamp > 3 || status.usb_millivolt > 4000;
    if (!status.is_usb_power_available)
    {
        status.power_draw_milliamp = -status.batt_milliamp;
    }
    if (status.power_draw_milliamp < 0)
    {
        ESP_LOGI(LOG_TAG, "power draw reads negative (%.2f) - this should not have happened", status.power_draw_milliamp);
        status.power_draw_milliamp = 0;
    }
    power_i2c_unlock();
}

void power_log_status()
{
    ESP_LOGI(LOG_TAG, "is_batt_charging : % d, is_usb_power_available : % d, usb_millivolt : % d, batt_millivolt : % d, batt_milliamp : % .2f, power_draw_milliamp : % .2f",
             status.is_batt_charging, status.is_usb_power_available, status.usb_millivolt, status.batt_millivolt, status.batt_milliamp, status.power_draw_milliamp);
}

void power_task_fun(void *_)
{
    unsigned long rounds = 0;
    while (true)
    {
        esp_task_wdt_reset();
        if (rounds % (POWER_STATUS_INTERVAL_MILLIS / POWER_TASK_INTERVAL_MILLIS) == 0)
        {
            power_read_status();
        }
        if (rounds % (POWER_LOG_STATUS_INTERVAL_MILLIS / POWER_TASK_INTERVAL_MILLIS) == 0)
        {
            power_log_status();
        }
        power_read_handle_lastest_irq();
        ++rounds;
        vTaskDelay(pdMS_TO_TICKS(POWER_TASK_INTERVAL_MILLIS));
    }
}
