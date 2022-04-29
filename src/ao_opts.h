#ifndef AOOPTS_H
#define AOOPTS_H

#ifdef __cplusplus
extern "C" {
#endif

long  ao_tick_ms_impl();
//unsigned int32_t ao_avail_heap_impl();

#ifdef __cplusplus
}
#endif

#ifndef ao_tick_ms
#define ao_tick_ms ao_tick_ms_impl
#endif

#ifndef ao_avail_heap
#define ao_avail_heap() 20000
#endif

//#ifndef AO_CONSOLE_PRINTF
//#define AO_CONSOLE_PRINTF(...) ESP_LOGI("[ocpp]", __VA_ARGS__)
//#endif

#ifndef AO_CUSTOM_CONSOLE_MAXMSGSIZE
#define AO_CUSTOM_CONSOLE_MAXMSGSIZE 192
#endif

#endif
