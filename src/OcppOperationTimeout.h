// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPOPERATIONTIMEOUT_H
#define OCPPOPERATIONTIMEOUT_H

#include <Arduino.h>

class Timeout {
protected:
    Timeout() = default;
public:
    virtual ~Timeout() = default;
    virtual void tick(bool sendingSuccessful) {}
    virtual void restart() {}
    virtual bool isExceeded() {return false;}
};

class FixedTimeout : public Timeout {
private:
    ulong TIMEOUT_DURATION;
    ulong timeout_start;
    bool timeout_active;
public:
    FixedTimeout(ulong TIMEOUT_EXPIRE);
    void tick(bool sendingSuccessful);
    void restart();
    bool isExceeded();
};

class OfflineSensitiveTimeout : public Timeout {
private:
    ulong TIMEOUT_DURATION;
    ulong timeout_start;
    ulong last_tick;
    bool timeout_active;
public:
    OfflineSensitiveTimeout(ulong TIMEOUT_EXPIRE);
    void tick(bool sendingSuccessful);
    void restart();
    bool isExceeded();
};

class SuppressedTimeout : public Timeout {
public:
    SuppressedTimeout() {}
    void tick(bool sendingSuccessful) {}
    void restart() {}
    bool isExceeded() {return false;}
};

#endif