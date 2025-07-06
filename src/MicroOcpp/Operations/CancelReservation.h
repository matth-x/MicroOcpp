// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CANCELRESERVATION_H
#define MO_CANCELRESERVATION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {
namespace v16 {

class ReservationService;

class CancelReservation : public Operation, public MemoryManaged {
private:
    ReservationService& reservationService;
    bool found = false;
    const char *errorCode = nullptr;
public:
    CancelReservation(ReservationService& reservationService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace v16
} //namespace MicroOcpp

#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
#endif
