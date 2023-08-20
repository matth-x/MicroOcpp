// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESERVENOW_H
#define RESERVENOW_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class ReserveNow : public Operation {
private:
    Model& model;
    const char *errorCode = nullptr;
    const char *reservationStatus = nullptr;
public:
    ReserveNow(Model& model);

    ~ReserveNow();

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
