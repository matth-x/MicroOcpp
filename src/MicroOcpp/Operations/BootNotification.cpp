// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/BootNotification.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Version.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

BootNotification::BootNotification(Context& context, BootService& bootService, HeartbeatService *heartbeatService, const MO_BootNotificationData& bnData) : MemoryManaged("v16/v201.Operation.", "BootNotification"), context(context), bootService(bootService), heartbeatService(heartbeatService), bnData(bnData), ocppVersion(context.getOcppVersion()) {
    
}

const char* BootNotification::getOperationType(){
    return "BootNotification";
}

std::unique_ptr<JsonDoc> BootNotification::createReq() {

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(9));
        JsonObject payload = doc->to<JsonObject>();
        if (bnData.chargePointModel) {payload["chargePointModel"] = bnData.chargePointModel;}
        if (bnData.chargePointVendor) {payload["chargePointVendor"] = bnData.chargePointVendor;}
        if (bnData.firmwareVersion) {payload["firmwareVersion"] = bnData.firmwareVersion;}
        if (bnData.chargePointSerialNumber) {payload["chargePointSerialNumber"] = bnData.chargePointSerialNumber;}
        if (bnData.meterSerialNumber) {payload["meterSerialNumber"] = bnData.meterSerialNumber;}
        if (bnData.meterType) {payload["meterType"] = bnData.meterType;}
        if (bnData.chargeBoxSerialNumber) {payload["chargeBoxSerialNumber"] = bnData.chargeBoxSerialNumber;}
        if (bnData.iccid) {payload["iccid"] = bnData.iccid;}
        if (bnData.imsi) {payload["imsi"] = bnData.imsi;}
        return doc;
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(2));
        JsonObject payload = doc->to<JsonObject>();
        payload["reason"] = "Unknown";
        JsonObject chargingStation = payload.createNestedObject("chargingStation");
        if (bnData.chargePointSerialNumber) {chargingStation["serialNumber"] = bnData.chargePointSerialNumber;}
        if (bnData.chargePointModel) {chargingStation["model"] = bnData.chargePointModel;}
        if (bnData.chargePointVendor) {chargingStation["vendorName"] = bnData.chargePointVendor;}
        if (bnData.firmwareVersion) {chargingStation["firmwareVersion"] = bnData.firmwareVersion;}
        if (bnData.iccid || bnData.imsi) {
            JsonObject modem = chargingStation.createNestedObject("modem");
            if (bnData.iccid) {modem["iccid"] = bnData.iccid;}
            if (bnData.imsi) {modem["imsi"] = bnData.imsi;}
        }
        return doc;
    }
    #endif //MO_ENABLE_V201

    MO_DBG_ERR("internal error");
    return createEmptyDocument();
}

void BootNotification::processConf(JsonObject payload) {
    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (context.getClock().setTime(currentTime)) {
            //success
        } else {
            MO_DBG_ERR("Time string format violation. Expect format like 2022-02-01T20:53:32.486Z");
            errorCode = "PropertyConstraintViolation";
            return;
        }
    } else {
        MO_DBG_ERR("Missing attribute currentTime");
        errorCode = "FormationViolation";
        return;
    }
    
    int interval = payload["interval"] | -1;
    if (interval < 0) {
        errorCode = "FormationViolation";
        return;
    }

    RegistrationStatus status = deserializeRegistrationStatus(payload["status"] | "Invalid");
    
    if (status == RegistrationStatus::UNDEFINED) {
        errorCode = "FormationViolation";
        return;
    }

    if (status == RegistrationStatus::Accepted) {
        if (heartbeatService) {
            heartbeatService->setHeartbeatInterval(interval);
        }
    }

    if (status != RegistrationStatus::Accepted) {
        bootService.setRetryInterval(interval);
    }
    bootService.notifyRegistrationStatus(status);

    MO_DBG_INFO("request has been %s", status == RegistrationStatus::Accepted ? "Accepted" :
                                       status == RegistrationStatus::Pending ? "replied with Pending" :
                                       "Rejected");
}

#if MO_ENABLE_MOCK_SERVER
int BootNotification::writeMockConf(const char *operationType, char *buf, size_t size, int userStatus, void *userData) {
    (void)userStatus;

    auto& context = *reinterpret_cast<Context*>(userData);

    char timeStr [MO_JSONDATE_SIZE];

    if (context.getClock().now().isUnixTime()) {
        context.getClock().toJsonString(context.getClock().now(), timeStr, sizeof(timeStr));
    } else {
        (void)snprintf(timeStr, sizeof(timeStr), "2025-05-18T18:55:13Z");
    }

    return snprintf(buf, size, "{\"currentTime\":\"%s\",\"interval\":60,\"status\":\"Accepted\"}", timeStr);
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
