// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESERVENOW_H
#define MO_RESERVENOW_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

class ReservationService;

class ReserveNow : public Operation, public MemoryManaged {
private:
    Context& context;
    ReservationService& rService;
    const char *errorCode = nullptr;
    const char *reservationStatus = nullptr;
public:
    ReserveNow(Context& context, ReservationService& rService);

    ~ReserveNow();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
#endif
