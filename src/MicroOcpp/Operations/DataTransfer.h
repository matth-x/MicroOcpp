// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DATATRANSFER_H
#define MO_DATATRANSFER_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Version.h>
#include <memory>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class DataTransfer : public Operation, public MemoryManaged {
private:
    Operation* op = nullptr;
    Context& context;
    char* msg = nullptr;
    const char* errorCode = nullptr;
public:
    DataTransfer(Context& context, const char *msg = nullptr);
    ~DataTransfer();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override { return errorCode;} 
};

} //namespace MicroOcpp
#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
