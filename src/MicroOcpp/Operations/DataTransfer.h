// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef DATATRANSFER_H
#define DATATRANSFER_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {
namespace Ocpp16 {

class DataTransfer : public Operation {
private:
    std::string msg {};
public:
    DataTransfer(const std::string &msg);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;
    
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
