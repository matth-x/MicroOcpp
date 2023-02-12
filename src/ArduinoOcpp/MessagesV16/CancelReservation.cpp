// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/CancelReservation.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Reservation/ReservationService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::CancelReservation;

CancelReservation::CancelReservation() {
  
}

const char* CancelReservation::getOcppOperationType(){
    return "CancelReservation";
}

void CancelReservation::processReq(JsonObject payload) {
    if (!payload.containsKey("reservationId")) {
        errorCode = "FormationViolation";
        return;
    }

    if (ocppModel && ocppModel->getReservationService()) {
        if (auto reservation = ocppModel->getReservationService()->getReservationById(payload["reservationId"])) {
            found = true;
            reservation->clear();
        }
    } else {
        errorCode = "InternalError";
    }
}

std::unique_ptr<DynamicJsonDocument> CancelReservation::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (found) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
