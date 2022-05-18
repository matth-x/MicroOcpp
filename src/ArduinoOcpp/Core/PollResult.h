// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef POLLRESULT_H
#define POLLRESULT_H

#include <utility>
#include <ArduinoOcpp/Debug.h>

namespace ArduinoOcpp {

template <class T>
class PollResult {
private:
    bool ready;
    T value;
public:
    PollResult() :          ready(false) {}
    PollResult(T&& value) : ready(true), value(value) {}
    PollResult(const PollResult<T>&) = delete;
    PollResult<T>& operator =(const PollResult<T>&) = delete;
    PollResult<T>& operator =(const PollResult<T>&& other) {
        ready = other.ready;
        value = std::move(other.value);
        return *this;
    }
    PollResult(PollResult<T>&& other) : ready(other.ready), value(std::move(other.value)) {}
    T&& toValue() {
        if (!ready) {
            AO_DBG_ERR("Not ready");
            (void)0;
        }
        ready = false;
        return std::move(value);
    }
    T& getValue() const {
        if (!ready) {
            AO_DBG_ERR("Not ready");
            (void)0;
        }
        return *value;
    }
    operator bool() const {return ready;}

    static PollResult<T> Await() {return PollResult<T>();}
};

}

#endif
