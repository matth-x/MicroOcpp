// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/Reset.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>

using ArduinoOcpp::Ocpp16::Reset;

Reset::Reset(OcppModel& context) : context(context) {
  
}

const char* Reset::getOcppOperationType(){
    return "Reset";
}

void Reset::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the device reset procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a reset (e.g. calling ESP.reset() )
     */
    bool isHard = !strcmp(payload["type"] | "undefined", "Hard");

    if (auto cpsService = context.getChargePointStatusService()) {
        if (!cpsService->getExecuteReset()) {
            AO_DBG_ERR("No reset handler set. Abort operation");
            (void)0;
            //resetAccepted remains false
        } else {
            if (!cpsService->getPreReset() || cpsService->getPreReset()(isHard) || isHard) {
                resetAccepted = true;
                cpsService->initiateReset(isHard);
                for (int i = 0; i < cpsService->getNumConnectors(); i++) {
                    auto connector = cpsService->getConnector(i);
                    if (connector) {
                        connector->endTransaction(isHard ? "HardReset" : "SoftReset");
                    }
                }
            }
        }
    } else {
        resetAccepted = true; //assume that onReceiveReset is set
    }
}

std::unique_ptr<DynamicJsonDocument> Reset::createConf() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = resetAccepted ? "Accepted" : "Rejected";
    return doc;
}
