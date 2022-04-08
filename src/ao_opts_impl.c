#include "ao_opts.h"

#include <stdio.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_event.h>

#include <freertos/task.h>
#include <freertos/FreeRTOSConfig.h>

#ifdef __cplusplus
extern "C" {
#endif

long ao_tick_ms_impl() {
    return xTaskGetTickCount() / configTICK_RATE_HZ;
}

#ifdef __cplusplus
}
#endif
