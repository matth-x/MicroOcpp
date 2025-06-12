// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_HEARTBEATSERVICE_H
#define MO_HEARTBEATSERVICE_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class Context;

#if MO_ENABLE_V16
namespace Ocpp16 {
class ConfigurationService;
class Configuration;
}
#endif
#if MO_ENABLE_V201
namespace Ocpp201 {
class VariableService;
class Variable;
}
#endif

class HeartbeatService : public MemoryManaged {
private:
    Context& context;

    int ocppVersion = -1;

    #if MO_ENABLE_V16
    Ocpp16::ConfigurationService *configService = nullptr;
    Ocpp16::Configuration *heartbeatIntervalIntV16 = nullptr;
    #endif
    #if MO_ENABLE_V201
    Ocpp201::VariableService *varService = nullptr;
    Ocpp201::Variable *heartbeatIntervalIntV201 = nullptr;
    #endif

    Timestamp lastHeartbeat;

public:
    HeartbeatService(Context& context);

    bool setup();

    void loop();

    bool setHeartbeatInterval(int interval);
};

bool validateHeartbeatInterval(int v, void*);

}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
