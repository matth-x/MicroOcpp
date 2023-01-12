// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppOperationTimeout.h>
#include <ArduinoOcpp/Platform.h>

using namespace ArduinoOcpp;

void Timeout::setOnTimeoutListener(OnTimeoutListener onTimeout) {
    if (onTimeout)
        onTimeoutListener = onTimeout;
}

void Timeout::setOnAbortListener(OnAbortListener onAbort) {
    if (onAbort)
        onAbortListener = onAbort;
}

void Timeout::trigger() {
    if (!triggered && timerIsExceeded()) {
        triggered = true;
        onTimeoutListener();
        onAbortListener();
    }
}
void Timeout::tick(bool sendingSuccessful) {
    timerTick(sendingSuccessful);
    trigger();
}
void Timeout::restart() {
    triggered = false;
    timerRestart();
}
bool Timeout::isExceeded() {
    bool exceeded = timerIsExceeded();
    trigger();
    return exceeded;
}

FixedTimeout::FixedTimeout(unsigned long TIMEOUT_DURATION) : TIMEOUT_DURATION(TIMEOUT_DURATION) { 
    timeout_active = false;
}

void FixedTimeout::timerTick(bool sendingSuccessful) {
    if (!timeout_active) {
        timeout_active = true;
        timeout_start = ao_tick_ms();
    }
}
void FixedTimeout::timerRestart() {
    timeout_start = ao_tick_ms();
    timeout_active = false;
}
bool FixedTimeout::timerIsExceeded() {
    return timeout_active && ao_tick_ms() - timeout_start >= TIMEOUT_DURATION;
}


OfflineSensitiveTimeout::OfflineSensitiveTimeout(unsigned long TIMEOUT_DURATION) : TIMEOUT_DURATION(TIMEOUT_DURATION) { 
    timeout_active = false;
}

void OfflineSensitiveTimeout::timerTick(bool sendingSuccessful) {
    unsigned long t = ao_tick_ms();
    
    if (!timeout_active) {
        timeout_active = true;
        timeout_start = t;
        last_tick = t;
    } else {
        if (!sendingSuccessful) {
            timeout_start += t - last_tick;
        }
    }

    last_tick = t;
}
void OfflineSensitiveTimeout::timerRestart() {
    unsigned long t = ao_tick_ms();
    timeout_start = t;
    last_tick = t;
    timeout_active = false;
}
bool OfflineSensitiveTimeout::timerIsExceeded() {
    return timeout_active && ao_tick_ms() - timeout_start >= TIMEOUT_DURATION;
}
