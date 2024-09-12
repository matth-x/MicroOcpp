// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Operations/StatusNotification.h>

using namespace MicroOcpp;

AvailabilityServiceEvse::AvailabilityServiceEvse(Context& context, unsigned int evseId) : MemoryManaged("v201.Availability.AvailabilityServiceEvse"), context(context), evseId(evseId) {

    snprintf(availabilityBoolKey, sizeof(availabilityBoolKey), MO_CONFIG_EXT_PREFIX "AVAIL_CONN_%d", evseId);
    availabilityBool = declareConfiguration<bool>(availabilityBoolKey, true, MO_KEYVALUE_FN, false, false, false);
}

void AvailabilityServiceEvse::loop() {

    if (evseId >= 1) {
        auto status = getStatus();

        if (status != reportedStatus &&
                context.getModel().getClock().now() >= MIN_TIME) {

            auto statusNotification = makeRequest(new Ocpp201::StatusNotification(evseId, status, context.getModel().getClock().now()));
            statusNotification->setTimeout(0);
            context.initiateRequest(std::move(statusNotification));
            reportedStatus = status;
            return;
        }
    }
}

void AvailabilityServiceEvse::setConnectorPluggedInput(std::function<bool()> connectorPluggedInput) {
    this->connectorPluggedInput = connectorPluggedInput;
}

void AvailabilityServiceEvse::setOccupiedInput(std::function<bool()> occupiedInput) {
    this->occupiedInput = occupiedInput;
}

ChargePointStatus AvailabilityServiceEvse::getStatus() {
    ChargePointStatus res = ChargePointStatus_UNDEFINED;

    /*
    * Handle special case: This is the Connector for the whole CP (i.e. evseId=0) --> only states Available, Unavailable, Faulted are possible
    */
    if (evseId == 0) {
        if (isFaulted()) {
            res = ChargePointStatus_Faulted;
        } else if (!isOperative()) {
            res = ChargePointStatus_Unavailable;
        } else {
            res = ChargePointStatus_Available;
        }
        return res;
    }

    if (isFaulted()) {
        res = ChargePointStatus_Faulted;
    } else if (!isOperative()) {
        res = ChargePointStatus_Unavailable;
    }
    #if MO_ENABLE_RESERVATION
    else if (context.getModel().getReservationService() && context.getModel().getReservationService()->getReservation(evseId)) {
        res = ChargePointStatus_Reserved;
    }
    #endif 
    else if ((!connectorPluggedInput || !connectorPluggedInput()) &&   //no vehicle plugged
               (!occupiedInput || !occupiedInput())) {                       //occupied override clear
        res = ChargePointStatus_Available;
    } else {
        res = ChargePointStatus_Occupied;
    }

    return res;
}

void AvailabilityServiceEvse::setInoperative(void *requesterId) {
    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (!inoperativeRequesters[i]) {
            inoperativeRequesters[i] = requesterId;
            return;
        }
    }
    MO_DBG_ERR("exceeded max. inoperative requesters");
}

void AvailabilityServiceEvse::resetInoperative(void *requesterId) {
    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (inoperativeRequesters[i] == requesterId) {
            inoperativeRequesters[i] = nullptr;
            return;
        }
    }
    MO_DBG_ERR("could not find inoperative requester");
}

void AvailabilityServiceEvse::setFaulted(void *requesterId) {
    for (size_t i = 0; i < MO_FAULTED_REQUESTERS_MAX; i++) {
        if (!faultedRequesters[i]) {
            faultedRequesters[i] = requesterId;
            return;
        }
    }
    MO_DBG_ERR("exceeded max. faulted requesters");
}

void AvailabilityServiceEvse::resetFaulted(void *requesterId) {
    for (size_t i = 0; i < MO_FAULTED_REQUESTERS_MAX; i++) {
        if (faultedRequesters[i] == requesterId) {
            faultedRequesters[i] = nullptr;
            return;
        }
    }
    MO_DBG_ERR("could not find faulted requester");
}

bool AvailabilityServiceEvse::isOperative() {
    if (availabilityBool && !availabilityBool->getBool()) {
        return false;
    }

    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (inoperativeRequesters[i]) {
            return false;
        }
    }
    return true;
}

bool AvailabilityServiceEvse::isFaulted() {
    for (size_t i = 0; i < MO_FAULTED_REQUESTERS_MAX; i++) {
        if (faultedRequesters[i]) {
            return true;
        }
    }
    return false;
}

AvailabilityService::AvailabilityService(Context& context, size_t numEvses) : MemoryManaged("v201.Availability.AvailabilityService"), context(context) {

    for (size_t i = 0; i < numEvses && i < MO_NUM_EVSE; i++) {
        evses[i] = new AvailabilityServiceEvse(context, (unsigned int)i);
    }

    context.getOperationRegistry().registerOperation("StatusNotification", [&context] () {
        return new Ocpp16::StatusNotification(-1, ChargePointStatus_UNDEFINED, Timestamp());});
}

AvailabilityService::~AvailabilityService() {
    for (size_t i = 0; evses[i]; i++) {
        delete evses[i];
    }
}

void AvailabilityService::loop() {
    for (size_t i = 0; evses[i]; i++) {
        evses[i]->loop();
    }
}

AvailabilityServiceEvse *AvailabilityService::getEvse(unsigned int evseId) {
    if (evseId >= MO_NUM_EVSE) {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }
    return evses[evseId];
}

#endif // MO_ENABLE_V201
