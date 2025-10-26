// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CUSTOMOPERATION_H
#define MO_CUSTOMOPERATION_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class CustomOperation : public Operation, public MemoryManaged {
private:
    const char *operationType = nullptr;
    void *userData = nullptr;
    void *userStatus = nullptr; //`onRequest` cb can write this number and pass status from `onRequest` to `writeResponse`

    //Operation initiated on this EVSE
    char *request = nullptr;
    void (*onResponse)(const char *payloadJson, void *userData) = nullptr;

    //Operation initiated by server
    void (*onRequest)(const char *operationType, const char *payloadJson, void **userStatus, void *userData) = nullptr;
    int (*writeResponse)(const char *operationType, char *buf, size_t size, void *userStatus, void *userData) = nullptr;
    void (*finally)(const char *operationType, void *userStatus, void *userData) = nullptr;
public:

    CustomOperation();

    ~CustomOperation();

    //for operations initiated at this device
    bool setupEvseInitiated(const char *operationType,
        const char *request,
        void (*onResponse)(const char *payloadJson, void *userData),
        void *userData);

    //for operations receied from remote
    bool setupCsmsInitiated(const char *operationType,
        void (*onRequest)(const char *operationType, const char *payloadJson, void **userStatus, void *userData),
        int (*writeResponse)(const char *operationType, char *buf, size_t size, void *userStatus, void *userData),
        void (*finally)(const char *operationType, void *userStatus, void *userData),
        void *userData);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace MicroOcpp
#endif
