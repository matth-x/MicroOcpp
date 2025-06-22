// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_HEARTBEAT_H
#define MO_HEARTBEAT_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class Context;

class Heartbeat : public Operation, public MemoryManaged {
private:
    Context& context;
public:
    Heartbeat(Context& context);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

#if MO_ENABLE_MOCK_SERVER
    static int writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData);
#endif
};

} //namespace MicroOcpp

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
