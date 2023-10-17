// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESETSERVICE_H
#define RESETSERVICE_H

#include <functional>

#include <MicroOcpp/Core/ConfigurationKeyValue.h>

namespace MicroOcpp {

class Context;

class ResetService {
private:
    Context& context;

    std::function<bool(bool isHard)> preReset; //true: reset is possible; false: reject reset; Await: need more time to determine
    std::function<void(bool isHard)> executeReset; //please disconnect WebSocket (MO remains initialized), shut down device and restart with normal initialization routine; on failure reconnect WebSocket
    unsigned int outstandingResetRetries = 0; //0 = do not reset device
    bool isHardReset = false;
    unsigned long t_resetRetry;

    std::shared_ptr<Configuration> resetRetriesInt;

public:
    ResetService(Context& context);

    ~ResetService();
    
    void loop();

    void setPreReset(std::function<bool(bool isHard)> preReset);
    std::function<bool(bool isHard)> getPreReset();

    void setExecuteReset(std::function<void(bool isHard)> executeReset);
    std::function<void(bool isHard)> getExecuteReset();

    void initiateReset(bool isHard);
};

} //end namespace MicroOcpp

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))

namespace MicroOcpp {

std::function<void(bool isHard)> makeDefaultResetFn();

}

#endif //MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))

#endif
