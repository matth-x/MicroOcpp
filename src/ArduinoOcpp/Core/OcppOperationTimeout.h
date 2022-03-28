// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OCPPOPERATIONTIMEOUT_H
#define OCPPOPERATIONTIMEOUT_H

#include <functional>

#include <sys/types.h>

namespace ArduinoOcpp {

using OnTimeoutListener = std::function<void()>;
using OnAbortListener = std::function<void()>;

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