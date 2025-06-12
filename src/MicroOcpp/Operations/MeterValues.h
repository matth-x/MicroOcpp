// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUES_H
#define MO_METERVALUES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;
struct MeterValue;

namespace Ocpp16 {

class MeterValues : public Operation, public MemoryManaged {
private:
    Context& context; //for adjusting the timestamp if MeterValue has been created before BootNotification
    MeterValue *meterValue = nullptr;
    bool isMeterValueOwner = false;

    unsigned int connectorId = 0;
    int transactionId = -1;

public:
    MeterValues(Context& context, unsigned int connectorId, int transactionId, MeterValue *meterValue, bool transferOwnership); //`transferOwnership`: if this object should take ownership of the passed `meterValue`

    MeterValues(Context& context); //for debugging only. Make this for the server pendant

    ~MeterValues();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
