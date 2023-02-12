// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/ReserveNow.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Reservation/ReservationService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Platform.h>

using ArduinoOcpp::Ocpp16::ReserveNow;

ReserveNow::ReserveNow() {
  
}

ReserveNow::~ReserveNow() {
  
}

const char* ReserveNow::getOcppOperationType(){
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

    OcppTimestamp validateTimestamp;
    if (!validateTimestamp.setTime(payload["expiryDate"])) {
        AO_DBG_WARN("bad time format");
        errorCode = "PropertyConstraintViolation";
        return;
    }

    int connectorId = payload["connectorId"];

    ReservationService *rService = nullptr;
    ChargePointStatusService *cpService = nullptr;
    if (ocppModel && ocppModel->getReservationService() &&
                ocppModel->getChargePointStatusService() && ocppModel->getChargePointStatusService()->getConnector(0)) {
        rService = ocppModel->getReservationService();
        cpService = ocppModel->getChargePointStatusService();
        ConnectorStatus *chargePoint = cpService->getConnector(0);

        if (connectorId >= cpService->getNumConnectors()) {
            errorCode = "PropertyConstraintViolation";
            return;
        }

        if (auto reservation = rService->getReservationById(payload["reservationId"])) {
            reservation->update(payload);
            reservationStatus = "Accepted";
        }

        ConnectorStatus *connector = nullptr;
        
        if (connectorId > 0) {
            connector = cpService->getConnector(connectorId);
        }

        if (chargePoint->inferenceStatus() == OcppEvseState::Faulted ||
                (connector && connector->inferenceStatus() == OcppEvseState::Faulted)) {
            reservationStatus = "Faulted";
            return;
        }

        if (chargePoint->getAvailability() != AVAILABILITY_OPERATIVE ||
                (connector && connector->getAvailability() == AVAILABILITY_INOPERATIVE)) {
            reservationStatus = "Unavailable";
            return;
        }

        if (connector && connector->inferenceStatus() != OcppEvseState::Available) {
            reservationStatus = "Occupied";
            return;
        }

        if (rService->updateReservation(payload)) {
            reservationStatus = "Accepted";
        } else {
            reservationStatus = "Occupied";
        }
    } else {
        errorCode = "InternalError";
    }
}

std::unique_ptr<DynamicJsonDocument> ReserveNow::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    if (reservationStatus) {
        payload["status"] = reservationStatus;
    } else {
        AO_DBG_ERR("didn't set reservationStatus");
        payload["status"] = "Rejected";
    }
    
    return doc;
}
