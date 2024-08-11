// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_HEARTBEATSERVICE_H
#define MO_HEARTBEATSERVICE_H

#include <memory>

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;

class HeartbeatService : public AllocOverrider {
private:
    Context& context;

    unsigned long lastHeartbeat;
    std::shared_ptr<Configuration> heartbeatIntervalInt;

public:
    HeartbeatService(Context& context);

    void loop();
};

}

#endif
