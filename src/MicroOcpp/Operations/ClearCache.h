// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCACHE_H
#define CLEARCACHE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {
namespace Ocpp16 {

class ClearCache : public Operation {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;
    bool success = true;
public:
    ClearCache(std::shared_ptr<FilesystemAdapter> filesystem);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
