// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Operations/Heartbeat.h>
#include <MicroOcpp/Platform.h>

using namespace MicroOcpp;

HeartbeatService::HeartbeatService(Context& context) : context(context) {
    heartbeatIntervalInt = declareConfiguration<int>("HeartbeatInterval", 86400);
    lastHeartbeat = mocpp_tick_ms();

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("Heartbeat", [&context] () {
        return new Ocpp16::Heartbeat(context.getModel());});
}

void HeartbeatService::loop() {
    unsigned long hbInterval = heartbeatIntervalInt->getInt();
    hbInterval *= 1000UL; //conversion s -> ms
    unsigned long now = mocpp_tick_ms();

    if (now - lastHeartbeat >= hbInterval) {
        lastHeartbeat = now;

        auto heartbeat = makeRequest(new Ocpp16::Heartbeat(context.getModel()));
        context.initiateRequest(std::move(heartbeat));
    }
}
