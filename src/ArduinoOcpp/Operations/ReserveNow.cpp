// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/ReserveNow.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/Reservation/ReservationService.h>
#include <ArduinoOcpp/Model/ChargeControl/Connector.h>
#include <ArduinoOcpp/Platform.h>

using ArduinoOcpp::Ocpp16::ReserveNow;

ReserveNow::ReserveNow(Model& model) : model(model) {
  
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

    Timestamp validateTimestamp;
    if (!validateTimestamp.setTime(payload["expiryDate"])) {
        AO_DBG_WARN("bad time format");
        errorCode = "PropertyConstraintViolation";
        return;
    }

    unsigned int connectorId = payload["connectorId"];

    if (model.getReservationService() &&
                model.getNumConnectors() > 0) {
        auto rService = model.getReservationService();
        auto chargePoint = model.getConnector(0);

        if (connectorId >= model.getNumConnectors()) {
            errorCode = "PropertyConstraintViolation";
            return;
        }

        auto reserveConnectorZeroSupported = declareConfiguration<bool>("ReserveConnectorZeroSupported", true, CONFIGURATION_VOLATILE);
        if (connectorId == 0 && (!reserveConnectorZeroSupported || !*reserveConnectorZeroSupported)) {
            reservationStatus = "Rejected";
            return;
        }

        if (auto reservation = rService->getReservationById(payload["reservationId"])) {
            reservation->update(payload);
            reservationStatus = "Accepted";
            return;
        }

        Connector *connector = nullptr;
        
        if (connectorId > 0) {
            connector = model.getConnector(connectorId);
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
