// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DATATRANSFERFRUNNINGCOST_H
#define MO_DATATRANSFERFRUNNINGCOST_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA

namespace MicroOcpp {
class Context;

namespace v16 {


class DataTransferRunningCost : public Operation, public MemoryManaged {
private:
    Context& context;
    const char *errorCode = nullptr;
public:
    DataTransferRunningCost(Context& context);
    ~DataTransferRunningCost();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override { return errorCode; }
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_CALIFORNIA
#endif //MO_ENABLE_V16
#endif //MO_DATATRANSFERFRUNNINGCOST_H
