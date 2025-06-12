// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CLEARCACHE_H
#define MO_CLEARCACHE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

namespace Ocpp16 {

class ClearCache : public Operation, public MemoryManaged {
private:
    MO_FilesystemAdapter *filesystem = nullptr;
    bool success = false;
public:
    ClearCache(MO_FilesystemAdapter *filesystem);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
