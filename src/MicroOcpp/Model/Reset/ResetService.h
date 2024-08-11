// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESETSERVICE_H
#define MO_RESETSERVICE_H

#include <functional>
#include <vector>

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Reset/ResetDefs.h>

namespace MicroOcpp {

class Context;

class ResetService : public AllocOverrider {
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

#if MO_ENABLE_V201

namespace MicroOcpp {

class Variable;

namespace Ocpp201 {

class ResetService : public AllocOverrider {
private:
    Context& context;

    struct Evse {
        Context& context;
        ResetService& resetService;
        const unsigned int evseId;

        std::function<bool(ResetType type)> notifyReset; //notify firmware about a Reset command. Return true if Reset is okay; false if Reset cannot be executed
        std::function<bool()> executeReset; //execute Reset of connector. Return true if Reset will be executed; false if there is a failure to Reset

        unsigned int outstandingResetRetries = 0; //0 = do not reset device
        unsigned long t_resetRetry;

        bool awaitTxStop = false;

        Evse(Context& context, ResetService& resetService, unsigned int evseId);

        void loop();
    };

    MemVector<Evse> evses;
    Evse *getEvse(unsigned int connectorId);
    Evse *getOrCreateEvse(unsigned int connectorId);

    Variable *resetRetriesInt = nullptr;

public:
    ResetService(Context& context);
    ~ResetService();
    
    void loop();

    void setNotifyReset(std::function<bool(ResetType)> notifyReset, unsigned int evseId = 0);
    std::function<bool(ResetType)> getNotifyReset(unsigned int evseId = 0);

    void setExecuteReset(std::function<bool()> executeReset, unsigned int evseId = 0);
    std::function<bool()> getExecuteReset(unsigned int evseId = 0);

    ResetStatus initiateReset(ResetType type, unsigned int evseId = 0);
};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201

#endif
