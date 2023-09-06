// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef GETDIAGNOSTICS_H
#define GETDIAGNOSTICS_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class GetDiagnostics : public Operation {
private:
    Model& model;
    std::string location {};
    int retries = 1;
    unsigned long retryInterval = 180;
    Timestamp startTime = Timestamp();
    Timestamp stopTime = Timestamp();

    std::string fileName {};
    
    bool formatError = false;
public:
    GetDiagnostics(Model& model);

    const char* getOperationType() override {return "GetDiagnostics";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return formatError ? "FormationViolation" : nullptr;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
