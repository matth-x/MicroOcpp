// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/Heartbeat/HeartbeatService.h>
#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Operations/Heartbeat.h>
#include <ArduinoOcpp/Platform.h>

using namespace ArduinoOcpp;

HeartbeatService::HeartbeatService(Context& context) : context(context) {
    heartbeatInterval = declareConfiguration("HeartbeatInterval", 86400);
    lastHeartbeat = ao_tick_ms();

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("Heartbeat", [&context] () {
        return new Ocpp16::Heartbeat(context.getModel());});
}

void HeartbeatService::loop() {
    unsigned long hbInterval = *heartbeatInterval;
    hbInterval *= 1000UL; //conversion s -> ms
    unsigned long now = ao_tick_ms();

    if (now - lastHeartbeat >= hbInterval) {
        lastHeartbeat = now;

        auto heartbeat = makeRequest(new Ocpp16::Heartbeat(context.getModel()));
        context.initiateRequest(std::move(heartbeat));
    }
}
