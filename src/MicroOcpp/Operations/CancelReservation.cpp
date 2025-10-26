// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

#include <MicroOcpp/Operations/CancelReservation.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

CancelReservation::CancelReservation(ReservationService& reservationService) : MemoryManaged("v16.Operation.", "CancelReservation"), reservationService(reservationService) {

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

std::unique_ptr<JsonDoc> CancelReservation::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (found) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}

#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
