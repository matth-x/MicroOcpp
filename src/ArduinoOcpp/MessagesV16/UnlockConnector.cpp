// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/UnlockConnector.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::UnlockConnector;

#define AO_UNLOCK_TIMEOUT 10000

UnlockConnector::UnlockConnector(OcppModel& context) : context(context) {
  
}

const char* UnlockConnector::getOcppOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {
    
    auto connectorId = payload["connectorId"] | -1;

    auto connector = context.getConnectorStatus(connectorId);

    if (!connector) {
        err = true;
        return;
    }

    connector->endSession("UnlockCommand");

    unlockConnector = connector->getOnUnlockConnector();
    if (unlockConnector != nullptr) {
        cbUnlockResult = unlockConnector();
    } else {
        AO_DBG_WARN("Unlock CB undefined");
    }

    timerStart = ao_tick_ms();
}

std::unique_ptr<DynamicJsonDocument> UnlockConnector::createConf() {
    if (!err && ao_tick_ms() - timerStart < AO_UNLOCK_TIMEOUT) {
        //do poll and if more time is needed, delay creation of conf msg

        if (unlockConnector) {
            if (!cbUnlockResult) {
                cbUnlockResult = unlockConnector();
                if (!cbUnlockResult) {
                    return nullptr; //no result yet - delay confirmation response
                }
            }
        }
    }

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (err || !unlockConnector) {
        payload["status"] = "NotSupported";
    } else if (cbUnlockResult && cbUnlockResult.toValue()) {
        payload["status"] = "Unlocked";
    } else {
        payload["status"] = "UnlockFailed";
    }
    return doc;
}
