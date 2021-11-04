// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/UpdateFirmware.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

using ArduinoOcpp::Ocpp16::UpdateFirmware;

UpdateFirmware::UpdateFirmware() {

}

void UpdateFirmware::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the FW update procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a FW update (e.g. calling ESPhttpUpdate.update(...) )
     */

    const char *loc = payload["location"] | "";
    location = String(loc);
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (location.isEmpty()) {
        formatError = true;
        Serial.println(F("[UpdateFirmware] Could not read location. Abort"));
        return;
    }

    //check the integrity of retrieveDate
    const char *retrieveDateRaw = payload["retrieveDate"] | "Invalid";
    if (!retreiveDate.setTime(retrieveDateRaw)) {
        formatError = true;
        Serial.println(F("[UpdateFirmware] Could not read retrieveDate. Abort"));
        return;
    }
    
    retries = payload["retries"] | 1;
    retryInterval = payload["retryInterval"] | 180;
}

DynamicJsonDocument* UpdateFirmware::createConf(){
    DynamicJsonDocument* doc = new DynamicJsonDocument(0);
    doc->to<JsonObject>();

    FirmwareService *fwService = getFirmwareService();
    if (fwService != NULL) {
        fwService->scheduleFirmwareUpdate(location, retreiveDate, retries, retryInterval);
    } else {
        Serial.println(F("[UpdateFirmware] FirmwareService has not been initialized before! Please have a look at ArduinoOcpp.cpp for an example. Abort"));
    }

    return doc;
}