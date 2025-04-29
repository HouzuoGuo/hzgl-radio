#pragma once

const int SUPERVISOR_WATCHDOG_TIMEOUT_SEC = 60;
const int SUPERVISOR_UNCONDITIONAL_RESTART_MILLIS = 60 * 60 * 1000;
const int SUPERVISOR_TASK_LOOP_INTERVAL_MILLIS = SUPERVISOR_WATCHDOG_TIMEOUT_SEC / 3 * 1000;
const int SUPERVISOR_FREE_MEM_REBOOT_THRESHOLD = 2048;

void supervisor_init();
void supervisor_health_check();
void supervisor_task_fun(void *_);