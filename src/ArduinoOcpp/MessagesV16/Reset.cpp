// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/Reset.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>

using ArduinoOcpp::Ocpp16::Reset;

Reset::Reset() {
  
}

const char* Reset::getOcppOperationType(){
    return "Reset";
}

void Reset::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the device reset procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a reset (e.g. calling ESP.reset() )
     */
    //const char *type = payload["type"] | "Invalid";

    if (ocppModel && ocppModel->getChargePointStatusService()) {
        auto cpsService = ocppModel->getChargePointStatusService();
        unsigned int connId = 0;
        for (unsigned int i = 0; i < cpsService->getNumConnectors(); i++) {
            auto connector = cpsService->getConnector(connId);
            if (connector) {
                connector->endSession();
            }
        }
    }
}

std::unique_ptr<DynamicJsonDocument> Reset::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Accepted";
    return doc;
}
