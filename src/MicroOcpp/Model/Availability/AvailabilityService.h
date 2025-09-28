// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs G01, G03, G04.
 *
 * G02 (Heartbeat) is implemented in the HeartbeatService
 */

#ifndef MO_AVAILABILITYSERVICE_H
#define MO_AVAILABILITYSERVICE_H

#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Model/Configuration/ConfigurationDefs.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

#ifdef __cplusplus

namespace MicroOcpp {

class Context;
class Clock;

namespace v16 {

class Model;
class Configuration;
class ConfigurationContainer;
class AvailabilityService;
class TransactionServiceEvse;

class AvailabilityServiceEvse : public MemoryManaged {
private:
    Context& context;
    Clock& clock;
    Model& model;
    AvailabilityService& availService;
    const unsigned int evseId;

    MO_Connection *connection = nullptr;
    TransactionServiceEvse *txServiceEvse = nullptr;

    bool (*connectorPluggedInput)(unsigned int evseId, void *userData) = nullptr;
    void  *connectorPluggedInputUserData = nullptr;
    bool (*evReadyInput)(unsigned int evseId, void *userData) = nullptr;
    void  *evReadyInputUserData = nullptr;
    bool (*evseReadyInput)(unsigned int evseId, void *userData) = nullptr;
    void  *evseReadyInputUserData = nullptr;
    bool (*occupiedInput)(unsigned int evseId, void *userData) = nullptr; //instead of Available, go into Preparing / Finishing state
    void  *occupiedInputUserData = nullptr;

    Configuration *availabilityBool = nullptr;
    char availabilityBoolKey [sizeof(MO_CONFIG_EXT_PREFIX "AVAIL_CONN_xxxx") + 1];
    bool availabilityVolatile = true;

    ConfigurationContainer *availabilityContainer = nullptr;

    Vector<MO_ErrorDataInput> errorDataInputs;
    Vector<bool> trackErrorDataInputs;
    int reportedErrorIndex = -1; //last reported error
    const char *getErrorCode();

    MO_ChargePointStatus currentStatus = MO_ChargePointStatus_UNDEFINED;
    MO_ChargePointStatus reportedStatus = MO_ChargePointStatus_UNDEFINED;
    Timestamp t_statusTransition;

    bool trackLoopExecute = false; //if loop has been executed once

public:
    AvailabilityServiceEvse(Context& context, AvailabilityService& availService, unsigned int evseId);
    ~AvailabilityServiceEvse();

    void setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData);
    void setEvReadyInput(bool (*evReady)(unsigned int, void*), void *userData);
    void setEvseReadyInput(bool (*evseReady)(unsigned int, void*), void *userData);
    void setOccupiedInput(bool (*occupied)(unsigned int, void*), void *userData);

    bool addErrorDataInput(MO_ErrorDataInput errorDataInput);

    void loop();

    bool setup();

    MO_ChargePointStatus getStatus();

    void setAvailability(bool available);
    void setAvailabilityVolatile(bool available); //set inoperative state but keep only until reboot at most

    ChangeAvailabilityStatus changeAvailability(bool operative);

    Operation *createTriggeredStatusNotification();

    bool isOperative();
    bool isFaulted();
};

class AvailabilityService : public MemoryManaged {
private:
    Context& context;

    AvailabilityServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    Configuration *minimumStatusDurationInt = nullptr; //in seconds

public:
    AvailabilityService(Context& context);
    ~AvailabilityService();

    bool setup();

    void loop();

    AvailabilityServiceEvse *getEvse(unsigned int evseId);

friend class AvailabilityServiceEvse;
};

} //namespace MicroOcpp
} //namespace v16
#endif //__cplusplus
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#ifndef MO_INOPERATIVE_REQUESTERS_MAX
#define MO_INOPERATIVE_REQUESTERS_MAX 3
#endif

#ifndef MO_FAULTED_REQUESTERS_MAX
#define MO_FAULTED_REQUESTERS_MAX 3
#endif

#ifdef __cplusplus

namespace MicroOcpp {

class Context;

namespace v201 {

class AvailabilityService;
class Variable;

struct FaultedInput {
    bool (*isFaulted)(unsigned int connectorId, void *userData) = nullptr;
    void *userData = nullptr;
};

class AvailabilityServiceEvse : public MemoryManaged {
private:
    Context& context;
    AvailabilityService& availService;
    const unsigned int evseId;

    bool (*connectorPluggedInput)(unsigned int evseId, void *userData) = nullptr;
    void  *connectorPluggedInputUserData = nullptr;
    bool (*occupiedInput)(unsigned int evseId, void *userData) = nullptr; //instead of Available, go into Occupied
    void  *occupiedInputUserData = nullptr;

    void *unavailableRequesters [MO_INOPERATIVE_REQUESTERS_MAX] = {nullptr};
    Vector<FaultedInput> faultedInputs;

    MO_ChargePointStatus reportedStatus = MO_ChargePointStatus_UNDEFINED;

    bool trackLoopExecute = false; //if loop has been executed once
public:
    AvailabilityServiceEvse(Context& context, AvailabilityService& availService, unsigned int evseId);

    void setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData);
    void setOccupiedInput(bool (*occupied)(unsigned int, void*), void *userData);

    bool addFaultedInput(FaultedInput faultedInput);

    void loop();

    MO_ChargePointStatus getStatus();

    void setUnavailable(void *requesterId);
    void setAvailable(void *requesterId);

    ChangeAvailabilityStatus changeAvailability(bool operative);

    Operation *createTriggeredStatusNotification();

    bool isAvailable();
    bool isOperative();
    bool isFaulted();
};

class AvailabilityService : public MemoryManaged {
private:
    Context& context;

    AvailabilityServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    Variable *offlineThreshold = nullptr;

public:
    AvailabilityService(Context& context);
    ~AvailabilityService();

    bool setup();

    void loop();

    AvailabilityServiceEvse *getEvse(unsigned int evseId);
};

} //namespace MicroOcpp
} //namespace v201
#endif //__cplusplus
#endif //MO_ENABLE_V201
#endif
