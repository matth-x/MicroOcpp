// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_RESERVATION

#include <MicroOcpp/Operations/CancelReservation.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::CancelReservation;
using MicroOcpp::MemJsonDoc;

CancelReservation::CancelReservation(ReservationService& reservationService) : AllocOverrider("v16.Operation.", getOperationType()), reservationService(reservationService) {
  
}

const char* CancelReservation::getOperationType() {
    return "CancelReservation";
}

void CancelReservation::processReq(JsonObject payload) {
    if (!payload.containsKey("reservationId")) {
        errorCode = "FormationViolation";
        return;
    }

    if (auto reservation = reservationService.getReservationById(payload["reservationId"])) {
        found = true;
        reservation->clear();
    }
}

std::unique_ptr<MemJsonDoc> CancelReservation::createConf(){
    auto doc = makeMemJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (found) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}

#endif //MO_ENABLE_RESERVATION
