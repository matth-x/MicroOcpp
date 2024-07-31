// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CUSTOMOPERATION_H
#define MO_CUSTOMOPERATION_H

#include <MicroOcpp/Core/Operation.h>

#include <functional>

namespace MicroOcpp {

namespace Ocpp16 {

class CustomOperation : public Operation {
private:
    std::string operationType;
    std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createReq;
    std::function<void (JsonObject)> fn_processConf;
    std::function<bool (const char*, const char*, JsonObject)> fn_processErr;  //optional
    std::function<void (JsonObject)> fn_processReq;
    std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createConf;
    std::function<const char* ()> fn_getErrorCode;            //optional
    std::function<const char* ()> fn_getErrorDescription;     //optional
    std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_getErrorDetails; //optional
public:

    //for operations initiated at this device
    CustomOperation(const char *operationType,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createReq,
            std::function<void (JsonObject)> fn_processConf,
            std::function<bool (const char*, const char*, JsonObject)> fn_processErr = nullptr);
    
    //for operations receied from remote
    CustomOperation(const char *operationType,
            std::function<void (JsonObject)> fn_processReq,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createConf,
            std::function<const char* ()> fn_getErrorCode = nullptr,
            std::function<const char* ()> fn_getErrorDescription = nullptr,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_getErrorDetails = nullptr);
    
    ~CustomOperation();

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    bool processErr(const char *code, const char *description, JsonObject details) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
    const char *getErrorCode() override;
    const char *getErrorDescription() override;
    std::unique_ptr<DynamicJsonDocument> getErrorDetails() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
