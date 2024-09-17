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

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <functional>

#include <MicroOcpp/Model/Availability/ChangeAvailabilityStatus.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointStatus.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Memory.h>

#ifndef MO_INOPERATIVE_REQUESTERS_MAX
#define MO_INOPERATIVE_REQUESTERS_MAX 3
#endif

#ifndef MO_FAULTED_REQUESTERS_MAX
#define MO_FAULTED_REQUESTERS_MAX 3
#endif

namespace MicroOcpp {

class Context;

class AvailabilityServiceEvse : public MemoryManaged {
private:
    Context& context;
    const unsigned int evseId;

    std::function<bool()> connectorPluggedInput;
    std::function<bool()> occupiedInput; //instead of Available, go into Occupied

    std::shared_ptr<Configuration> availabilityBool;
    char availabilityBoolKey [sizeof(MO_CONFIG_EXT_PREFIX "AVAIL_CONN_xxxx") + 1];
    void *inoperativeRequesters [MO_INOPERATIVE_REQUESTERS_MAX] = {nullptr};
    void *faultedRequesters [MO_FAULTED_REQUESTERS_MAX] = {nullptr};

    ChargePointStatus reportedStatus = ChargePointStatus_UNDEFINED;
public:
    AvailabilityServiceEvse(Context& context, unsigned int evseId);

    void loop();

    void setConnectorPluggedInput(std::function<bool()> connectorPluggedInput);
    void setOccupiedInput(std::function<bool()> occupiedInput);

    ChargePointStatus getStatus();

    void setInoperative(void *requesterId);
    void resetInoperative(void *requesterId);

    ChangeAvailabilityStatus changeAvailability(bool operative);

    void setFaulted(void *requesterId);
    void resetFaulted(void *requesterId);

    bool isOperative();
    bool isFaulted();
};

class AvailabilityService : public MemoryManaged {
private:
    Context& context;

    AvailabilityServiceEvse* evses [MO_NUM_EVSE] = {nullptr};

public:
    AvailabilityService(Context& context, size_t numEvses);
    ~AvailabilityService();

    void loop();

    AvailabilityServiceEvse *getEvse(unsigned int evseId);

    ChangeAvailabilityStatus changeAvailability(bool operative);
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201

#endif
