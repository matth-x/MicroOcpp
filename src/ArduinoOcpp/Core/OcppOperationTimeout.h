// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPOPERATIONTIMEOUT_H
#define OCPPOPERATIONTIMEOUT_H

#include <Arduino.h>

namespace ArduinoOcpp {

typedef std::function<void()> OnTimeoutListener;
typedef std::function<void()> OnAbortListener;

class Timeout {
private:
    OnTimeoutListener onTimeoutListener = [] () {};
    OnAbortListener onAbortListener = [] () {};
    bool triggered = false;
    void trigger();
protected:
    Timeout() = default;
public:
    void setOnTimeoutListener(OnTimeoutListener onTimeoutListener);
    void setOnAbortListener(OnAbortListener onAbortListener);
    virtual ~Timeout() = default;
    void tick(bool sendingSuccessful);
    virtual void timerTick(bool sendingSuccessful) = 0;
    void restart();
    virtual void timerRestart() = 0;
    bool isExceeded();
    virtual bool timerIsExceeded() = 0;
};

class FixedTimeout : public Timeout {
private:
    ulong TIMEOUT_DURATION;
    ulong timeout_start;
    bool timeout_active;
public:
    FixedTimeout(ulong TIMEOUT_EXPIRE);
    void timerTick(bool sendingSuccessful);
    void timerRestart();
    bool timerIsExceeded();
};

class OfflineSensitiveTimeout : public Timeout {
private:
    ulong TIMEOUT_DURATION;
    ulong timeout_start;
    ulong last_tick;
    bool timeout_active;
public:
    OfflineSensitiveTimeout(ulong TIMEOUT_EXPIRE);
    void timerTick(bool sendingSuccessful);
    void timerRestart();
    bool timerIsExceeded();
};

class SuppressedTimeout : public Timeout {
public:
    SuppressedTimeout() = default;
    void timerTick(bool sendingSuccessful) {}
    void timerRestart() {}
    bool timerIsExceeded() {return false;}
};

} //end namespace ArduinoOcpp
#endif