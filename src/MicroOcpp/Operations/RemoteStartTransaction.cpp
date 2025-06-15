// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License


#include <MicroOcpp/Operations/RemoteStartTransaction.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

RemoteStartTransaction::RemoteStartTransaction(Context& context, RemoteControlService& rcService) : MemoryManaged("v16.Operation.", "RemoteStartTransaction"), context(context), rcService(rcService) {
  
}

const char* RemoteStartTransaction::getOperationType() {
    return "RemoteStartTransaction";
}

void RemoteStartTransaction::processReq(JsonObject payload) {
    int connectorId = payload["connectorId"] | -1;

    if (!payload.containsKey("idTag")) {
        errorCode = "FormationViolation";
        return;
    }

    const char *idTag = payload["idTag"] | "";
    size_t len = strnlen(idTag, IDTAG_LEN_MAX + 1);
    if (len == 0 || len > IDTAG_LEN_MAX) {
        errorCode = "PropertyConstraintViolation";
        errorDescription = "idTag empty or too long";
        return;
    }

    std::unique_ptr<ChargingProfile> chargingProfile;

    if (payload.containsKey("chargingProfile") && context.getModel16().getSmartChargingService()) {
        MO_DBG_INFO("Setting Charging profile via RemoteStartTransaction");

        chargingProfile = std::unique_ptr<ChargingProfile>(new ChargingProfile());
        if (!chargingProfile) {
            MO_DBG_ERR("OOM");
            errorCode = "InternalError";
            return;
        }

        bool valid = chargingProfile->parseJson(context.getClock(), MO_OCPP_V16, payload["chargingProfile"]);
        if (!valid) {
            errorCode = "FormationViolation";
            errorDescription = "chargingProfile validation failed";
            return;
        }

        if (!chargingProfile) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "chargingProfile validation failed";
            return;
        }

        if (chargingProfile->chargingProfilePurpose != ChargingProfilePurposeType::TxProfile) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Can only set TxProfile here";
            return;
        }

        if (chargingProfile->chargingProfileId < 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "RemoteStartTx profile requires non-negative chargingProfileId";
            return;
        }
    }

    status = rcService.remoteStartTransaction(connectorId, idTag, std::move(chargingProfile));
    if (status == Ocpp16::RemoteStartStopStatus::ERR_INTERNAL) {
        errorCode = "InternalError";
        return;
    }
}

std::unique_ptr<JsonDoc> RemoteStartTransaction::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (status == Ocpp16::RemoteStartStopStatus::Accepted) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}

#endif //MO_ENABLE_V16
