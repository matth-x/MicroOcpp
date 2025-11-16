// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESETSERVICE_H
#define MO_RESETSERVICE_H

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Reset/ResetDefs.h>
#include <MicroOcpp/Model/Common/EvseId.h>

#if MO_ENABLE_V16

#ifdef __cplusplus

namespace MicroOcpp {

class Context;

namespace v16 {

class Configuration;

class ResetService : public MemoryManaged {
private:
    Context& context;

    bool (*notifyResetCb)(bool isHard, void *userData) = nullptr; //true: reset is possible; false: reject reset; Await: need more time to determine
    void *notifyResetUserData = nullptr;
    bool (*executeResetCb)(bool isHard, void *userData) = nullptr;; //please disconnect WebSocket (MO remains initialized), shut down device and restart with normal initialization routine; on failure reconnect WebSocket
    void *executeResetUserData = nullptr;
    unsigned int outstandingResetRetries = 0; //0 = do not reset device
    bool isHardReset = false;
    Timestamp lastResetAttempt;

    Configuration *resetRetriesInt = nullptr;

public:
    ResetService(Context& context);

    ~ResetService();

    void setNotifyReset(bool (*notifyReset)(bool isHard, void *userData), void *userData);

    void setExecuteReset(bool (*executeReset)(bool isHard, void *userData), void *userData);

    bool setup();

    void loop();

    bool isPreResetDefined();
    bool notifyReset(bool isHard);

    bool isExecuteResetDefined();

    void initiateReset(bool isHard);
};

} //namespace v16
} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#ifdef __cplusplus

namespace MicroOcpp {

namespace v201 {

class Variable;

class ResetService : public MemoryManaged {
private:
    Context& context;

    struct Evse : public MemoryManaged {
        Context& context;
        ResetService& resetService;
        const unsigned int evseId;

        bool (*notifyReset)(MO_ResetType, unsigned int evseId, void *userData) = nullptr; //notify firmware about a Reset command. Return true if Reset is okay; false if Reset cannot be executed
        void *notifyResetUserData = nullptr;
        bool (*executeReset)(unsigned int evseId, void *userData) = nullptr; //execute Reset of connector. Return true if Reset will be executed; false if there is a failure to Reset
        void *executeResetUserData = nullptr;

        unsigned int outstandingResetRetries = 0; //0 = do not reset device
        Timestamp lastResetAttempt;

        bool awaitTxStop = false;

        Evse(Context& context, ResetService& resetService, unsigned int evseId);

        bool setup();

        void loop();
    };

    Evse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;
    Evse *getEvse(unsigned int connectorId);

    Variable *resetRetriesInt = nullptr;

public:
    ResetService(Context& context);
    ~ResetService();

    void setNotifyReset(unsigned int evseId, bool (*notifyReset)(MO_ResetType, unsigned int evseId, void *userData), void *userData);

    void setExecuteReset(unsigned int evseId, bool (*executeReset)(unsigned int evseId, void *userData), void *userData);

    bool setup();

    void loop();

    MO_ResetStatus initiateReset(MO_ResetType type, unsigned int evseId = 0);
};

} //namespace v201
} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_ENABLE_V201

#endif
