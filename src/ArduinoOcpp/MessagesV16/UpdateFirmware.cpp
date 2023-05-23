// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/UpdateFirmware.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::UpdateFirmware;

UpdateFirmware::UpdateFirmware(Model& model) : model(model) {

}

void UpdateFirmware::processReq(JsonObject payload) {
    /*
     * Process the application data here. Note: you have to implement the FW update procedure in your client code. You have to set
     * a onSendConfListener in which you initiate a FW update (e.g. calling ESPhttpUpdate.update(...) )
     */

    const char *loc = payload["location"] | "";
    location = loc;
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (location.empty()) {
        formatError = true;
        AO_DBG_WARN("Could not read location. Abort");
        return;
    }

    //check the integrity of retrieveDate
    const char *retrieveDateRaw = payload["retrieveDate"] | "Invalid";
    if (!retreiveDate.setTime(retrieveDateRaw)) {
        formatError = true;
        AO_DBG_WARN("Could not read retrieveDate. Abort");
        return;
    }
    
    retries = payload["retries"] | 1;
    retryInterval = payload["retryInterval"] | 180;
}

std::unique_ptr<DynamicJsonDocument> UpdateFirmware::createConf(){
    if (auto fwService = model.getFirmwareService()) {
        fwService->scheduleFirmwareUpdate(location, retreiveDate, retries, retryInterval);
    } else {
        AO_DBG_ERR("FirmwareService has not been initialized before! Please have a look at ArduinoOcpp.cpp for an example. Abort");
    }

    return createEmptyDocument();
}
