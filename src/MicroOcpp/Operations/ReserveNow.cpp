// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ReserveNow.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

ReserveNow::ReserveNow(Context& context, ReservationService& rService) : MemoryManaged("v16.Operation.", "ReserveNow"), context(context), rService(rService) {

}

ReserveNow::~ReserveNow() {

}

const char* ReserveNow::getOperationType(){
    return "ReserveNow";
}

void ReserveNow::processReq(JsonObject payload) {
    if (!payload.containsKey("connectorId") ||
            payload["connectorId"] < 0 ||
            !payload.containsKey("expiryDate") ||
            !payload.containsKey("idTag") ||
            //parentIdTag is optional
            !payload.containsKey("reservationId")) {
        errorCode = "FormationViolation";
        return;
    }

    int connectorId = payload["connectorId"] | -1;
    if (connectorId < 0 || (unsigned int) connectorId >= context.getModel16().getNumEvseId()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    Timestamp expiryDate;
    if (!context.getClock().parseString(payload["expiryDate"] | "_Undefined", expiryDate)) {
        MO_DBG_WARN("bad time format");
        errorCode = "PropertyConstraintViolation";
        return;
    }

    const char *idTag = payload["idTag"] | "";
    if (!*idTag) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    const char *parentIdTag = nullptr;
    if (payload.containsKey("parentIdTag")) {
        parentIdTag = payload["parentIdTag"];
    }

    int reservationId = payload["reservationId"] | -1;

    auto availSvc = context.getModel16().getAvailabilityService();
    auto availSvcCp = availSvc ? availSvc->getEvse(0) : nullptr;
    if (!availSvcCp) {
        errorCode = "InternalError";
        return;
    }

    AvailabilityServiceEvse *availSvcEvse = nullptr;
    if (connectorId > 0) {
        availSvcEvse = availSvc->getEvse(connectorId);
        if (!availSvcEvse) {
            errorCode = "InternalError";
            return;
        }
    }

    auto configServce = context.getModel16().getConfigurationService();
    auto reserveConnectorZeroSupportedBool = configServce ? configServce->declareConfiguration<bool>("ReserveConnectorZeroSupported", true, MO_CONFIGURATION_VOLATILE) : nullptr;
    if (!reserveConnectorZeroSupportedBool) {
        errorCode = "InternalError";
        return;
    }

    if (connectorId == 0 && !reserveConnectorZeroSupportedBool->getBool()) {
        reservationStatus = "Rejected";
        return;
    }

    if (auto reservation = rService.getReservationById(reservationId)) {
        reservation->update(reservationId, (unsigned int) connectorId, expiryDate, idTag, parentIdTag);
        reservationStatus = "Accepted";
        return;
    }

    if (availSvcCp->getStatus() == MO_ChargePointStatus_Faulted ||
            (availSvcEvse && availSvcEvse->getStatus() == MO_ChargePointStatus_Faulted)) {
        reservationStatus = "Faulted";
        return;
    }

    if (availSvcCp->getStatus() == MO_ChargePointStatus_Unavailable ||
            (availSvcEvse && availSvcEvse->getStatus() == MO_ChargePointStatus_Unavailable)) {
        reservationStatus = "Unavailable";
        return;
    }

    if (availSvcEvse && availSvcEvse->getStatus() != MO_ChargePointStatus_Available) {
        reservationStatus = "Occupied";
        return;
    }

    if (rService.updateReservation(reservationId, (unsigned int) connectorId, expiryDate, idTag, parentIdTag)) {
        reservationStatus = "Accepted";
    } else {
        reservationStatus = "Occupied";
    }
}

std::unique_ptr<JsonDoc> ReserveNow::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    if (reservationStatus) {
        payload["status"] = reservationStatus;
    } else {
        MO_DBG_ERR("didn't set reservationStatus");
        payload["status"] = "Rejected";
    }
    
    return doc;
}

#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
