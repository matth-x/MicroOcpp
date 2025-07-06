// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DATATRANSFER_H
#define MO_DATATRANSFER_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace v16 {

class DataTransfer : public Operation, public MemoryManaged {
private:
    char *msg = nullptr;
public:
    DataTransfer(const char *msg = nullptr);
    ~DataTransfer();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
    
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
