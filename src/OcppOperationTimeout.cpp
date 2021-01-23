// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "OcppOperationTimeout.h"

FixedTimeout::FixedTimeout(ulong TIMEOUT_DURATION) : TIMEOUT_DURATION(TIMEOUT_DURATION) { 
    timeout_active = false;
}

void FixedTimeout::tick(bool sendingSuccessful) {
    if (!timeout_active) {
        timeout_active = true;
        timeout_start = millis();
    }
}
void FixedTimeout::restart() {
    timeout_start = millis();
    timeout_active = false;
}
bool FixedTimeout::isExceeded() {
    return timeout_active && millis() - timeout_start >= TIMEOUT_DURATION;
}


OfflineSensitiveTimeout::OfflineSensitiveTimeout(ulong TIMEOUT_DURATION) : TIMEOUT_DURATION(TIMEOUT_DURATION) { 
    timeout_active = false;
}

void OfflineSensitiveTimeout::tick(bool sendingSuccessful) {
    ulong t = millis();
    
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
void OfflineSensitiveTimeout::restart() {
    ulong t = millis();
    timeout_start = t;
    last_tick = t;
    timeout_active = false;
}
bool OfflineSensitiveTimeout::isExceeded() {
    return timeout_active && millis() - timeout_start >= TIMEOUT_DURATION;
}
