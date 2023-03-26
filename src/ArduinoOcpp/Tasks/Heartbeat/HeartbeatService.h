// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef HEARTBEATSERVICE_H
#define HEARTBEATSERVICE_H

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <memory>

namespace ArduinoOcpp {

class OcppEngine;

class HeartbeatService {
private:
    OcppEngine& context;

    unsigned long lastHeartbeat;
    std::shared_ptr<ArduinoOcpp::Configuration<int>> heartbeatInterval;

public:
    HeartbeatService(OcppEngine& context);

    void loop();
};

}

#endif
