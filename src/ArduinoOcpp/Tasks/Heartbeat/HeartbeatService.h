// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef HEARTBEATSERVICE_H
#define HEARTBEATSERVICE_H

#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Core/Configuration.h>

namespace ArduinoOcpp {
    class HeartbeatService {
    private:
        ulong lastHeartbeat;
        std::shared_ptr<ArduinoOcpp::Configuration<int>> heartbeatInterval;
    public:
        HeartbeatService(int interval);

        void loop();
    };
}

#endif
