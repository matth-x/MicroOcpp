// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef HEARTBEATSERVICE_H
#define HEARTBEATSERVICE_H

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <memory>

namespace MicroOcpp {

class Context;

class HeartbeatService {
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
