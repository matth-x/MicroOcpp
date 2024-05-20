// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CANCELRESERVATION_H
#define MO_CANCELRESERVATION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_RESERVATION

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class ReservationService;

namespace Ocpp16 {

class CancelReservation : public Operation {
private:
    ReservationService& reservationService;
    bool found = false;
    const char *errorCode = nullptr;
public:
    CancelReservation(ReservationService& reservationService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif //MO_ENABLE_RESERVATION
#endif
