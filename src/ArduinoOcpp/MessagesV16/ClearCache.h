// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCACHE_H
#define CLEARCACHE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class ClearCache : public OcppMessage {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;
    bool success = true;
public:
    ClearCache(std::shared_ptr<FilesystemAdapter> filesystem);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
