// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/Heartbeat.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

HeartbeatService::HeartbeatService(Context& context) : MemoryManaged("v16/v201.Heartbeat.HeartbeatService"), context(context) {

}

bool HeartbeatService::setup() {

    ocppVersion = context.getOcppVersion();

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {

        configService = context.getModel16().getConfigurationService();
        if (!configService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        //if transactions can start before the BootNotification succeeds
        heartbeatIntervalIntV16 = configService->declareConfiguration<int>("HeartbeatInterval", 86400);
        if (!heartbeatIntervalIntV16) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        if (!configService->registerValidator<int>("HeartbeatInterval", validateHeartbeatInterval)) {
            MO_DBG_ERR("initialization error");
            return false;
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {

        varService = context.getModel201().getVariableService();
        if (!varService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        //if transactions can start before the BootNotification succeeds
        heartbeatIntervalIntV201 = varService->declareVariable<int>("OCPPCommCtrlr", "HeartbeatInterval", 86400);
        if (!heartbeatIntervalIntV201) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        if (!varService->registerValidator<int>("OCPPCommCtrlr", "HeartbeatInterval", validateHeartbeatInterval)) {
            MO_DBG_ERR("initialization error");
            return false;
        }
    }
    #endif //MO_ENABLE_V201

    lastHeartbeat = context.getClock().getUptime();

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("Heartbeat", nullptr, Heartbeat::writeMockConf, nullptr, reinterpret_cast<void*>(&context));
    #endif //MO_ENABLE_MOCK_SERVER

    auto rcService = context.getModelCommon().getRemoteControlService();
    if (!rcService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    rcService->addTriggerMessageHandler("Heartbeat", [] (Context& context) -> Operation* {
        return new Heartbeat(context);});

    return true;
}

void HeartbeatService::loop() {

    int hbInterval = 86400;
    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        hbInterval = heartbeatIntervalIntV16->getInt();
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        hbInterval = heartbeatIntervalIntV201->getInt();
    }
    #endif //MO_ENABLE_V201

    int32_t dtLastHeartbeat;
    if (!context.getClock().delta(context.getClock().getUptime(), lastHeartbeat, dtLastHeartbeat)) {
        dtLastHeartbeat = hbInterval; //force send Heartbeat
    }

    if (dtLastHeartbeat >= hbInterval && hbInterval > 0) {
        lastHeartbeat = context.getClock().getUptime();

        auto heartbeat = makeRequest(context, new Heartbeat(context));
        // Heartbeats can not deviate more than 4s from the configured interval
        heartbeat->setTimeout(std::min(4, hbInterval));
        context.getMessageService().sendRequest(std::move(heartbeat));
    }
}

bool HeartbeatService::setHeartbeatInterval(int interval) {
    if (!validateHeartbeatInterval(interval, nullptr)) {
        return false;
    }

    bool success = false;

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        heartbeatIntervalIntV16->setInt(interval);
        success = configService->commit();
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        heartbeatIntervalIntV201->setInt(interval);
        success = varService->commit();
    }
    #endif //MO_ENABLE_V201

    return success;
}

bool MicroOcpp::validateHeartbeatInterval(int v, void*) {
    return v >= 1;
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
