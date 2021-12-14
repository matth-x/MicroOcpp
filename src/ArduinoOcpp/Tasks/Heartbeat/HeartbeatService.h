// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef HEARTBEATSERVICE_H
#define HEARTBEATSERVICE_H

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>

namespace ArduinoOcpp {

class OcppEngine;

class HeartbeatService {
private:
    OcppEngine& context;

    ulong lastHeartbeat;
    std::shared_ptr<ArduinoOcpp::Configuration<int>> heartbeatInterval;

public:
    HeartbeatService(OcppEngine& context);

    void loop();
};

}

#endif
