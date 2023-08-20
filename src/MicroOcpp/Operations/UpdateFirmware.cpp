// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/UpdateFirmware.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UpdateFirmware;

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
        MOCPP_DBG_WARN("Could not read location. Abort");
        return;
    }

    //check the integrity of retrieveDate
    const char *retrieveDateRaw = payload["retrieveDate"] | "Invalid";
    if (!retreiveDate.setTime(retrieveDateRaw)) {
        formatError = true;
        MOCPP_DBG_WARN("Could not read retrieveDate. Abort");
        return;
    }
    
    retries = payload["retries"] | 1;
    retryInterval = payload["retryInterval"] | 180;
}

std::unique_ptr<DynamicJsonDocument> UpdateFirmware::createConf(){
    if (auto fwService = model.getFirmwareService()) {
        fwService->scheduleFirmwareUpdate(location, retreiveDate, retries, retryInterval);
    } else {
        MOCPP_DBG_ERR("FirmwareService has not been initialized before! Please have a look at MicroOcpp.cpp for an example. Abort");
    }

    return createEmptyDocument();
}
