// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/Heartbeat/HeartbeatService.h>
#include <ArduinoOcpp/MessagesV16/Heartbeat.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

using namespace ArduinoOcpp;

HeartbeatService::HeartbeatService(int interval) {
    heartbeatInterval = declareConfiguration("HeartbeatInterval", interval);
    *heartbeatInterval = interval;
    lastHeartbeat = millis();
}

void HeartbeatService::loop() {
    ulong hbInterval = *heartbeatInterval;
    hbInterval *= 1000UL; //conversion s -> ms
    ulong now = millis();

    if (now - lastHeartbeat >= hbInterval) {
        lastHeartbeat = now;

        OcppOperation *heartbeat = makeOcppOperation("Heartbeat");
        initiateOcppOperation(heartbeat);
    }
}
