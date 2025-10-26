// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UpdateFirmware.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

UpdateFirmware::UpdateFirmware(Context& context, FirmwareService& fwService) : MemoryManaged("v16.Operation.", "UpdateFirmware"), context(context), fwService(fwService) {

}

void UpdateFirmware::processReq(JsonObject payload) {

    const char *location = payload["location"] | "";
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (!*location) {
        errorCode = "FormationViolation";
        return;
    }

    int retries = payload["retries"] | 1;
    int retryInterval = payload["retryInterval"] | 180;
    if (retries < 0 || retryInterval < 0) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    //check the integrity of retrieveDate
    if (!payload.containsKey("retrieveDate")) {
        errorCode = "FormationViolation";
        return;
    }

    Timestamp retrieveDate;
    if (!context.getClock().parseString(payload["retrieveDate"] | "_Unvalid", retrieveDate)) {
        errorCode = "FormationViolation";
        MO_DBG_WARN("bad time format");
        return;
    }

    fwService.scheduleFirmwareUpdate(location, retrieveDate, (unsigned int) retries, (unsigned int) retryInterval);
}

std::unique_ptr<JsonDoc> UpdateFirmware::createConf(){
    return createEmptyDocument();
}

#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
