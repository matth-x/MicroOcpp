// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef POLLRESULT_H
#define POLLRESULT_H

#include <utility>
#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

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
            MO_DBG_ERR("Not ready");
            (void)0;
        }
        ready = false;
        return std::move(value);
    }
    T& getValue() const {
        if (!ready) {
            MO_DBG_ERR("Not ready");
            (void)0;
        }
        return *value;
    }
    operator bool() const {return ready;}

    static PollResult<T> Await() {return PollResult<T>();}
};

}

#endif
