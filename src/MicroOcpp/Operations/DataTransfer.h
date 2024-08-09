// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DATATRANSFER_H
#define MO_DATATRANSFER_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {
namespace Ocpp16 {

class DataTransfer : public Operation, public AllocOverrider {
private:
    MemString msg;
public:
    DataTransfer();
    DataTransfer(const MemString &msg);

    const char* getOperationType() override;

    std::unique_ptr<MemJsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;
    
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
