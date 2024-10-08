// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/SetChargingProfile.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::SetChargingProfile;
using MicroOcpp::JsonDoc;

SetChargingProfile::SetChargingProfile(Model& model, SmartChargingService& scService) : MemoryManaged("v16.Operation.", "SetChargingProfile"), model(model), scService(scService) {

}

SetChargingProfile::~SetChargingProfile() {

}

const char* SetChargingProfile::getOperationType(){
    return "SetChargingProfile";
}

void SetChargingProfile::processReq(JsonObject payload) {

    int connectorId = payload["connectorId"] | -1;
    if (connectorId < 0 || !payload.containsKey("csChargingProfiles")) {
        errorCode = "FormationViolation";
        return;
    }

    if ((unsigned int) connectorId >= model.getNumConnectors()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    JsonObject csChargingProfiles = payload["csChargingProfiles"];

    auto chargingProfile = loadChargingProfile(csChargingProfiles);
    if (!chargingProfile) {
        errorCode = "PropertyConstraintViolation";
        errorDescription = "csChargingProfiles validation failed";
        return;
    }

    if (chargingProfile->getChargingProfilePurpose() == ChargingProfilePurposeType::TxProfile) {
        // if TxProfile, check if a transaction is running

        if (connectorId == 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Cannot set TxProfile at connectorId 0";
            return;
        }
        Connector *connector = model.getConnector(connectorId);
        if (!connector) {
            errorCode = "PropertyConstraintViolation";
            return;
        }

        auto& transaction = connector->getTransaction();
        if (!transaction || !connector->getTransaction()->isRunning()) {
            //no transaction running, reject profile
            accepted = false;
            return;
        }

        if (chargingProfile->getTransactionId() >= 0 &&
                chargingProfile->getTransactionId() != transaction->getTransactionId()) {
            //transactionId undefined / mismatch
            accepted = false;
            return;
        }

        //seems good
    } else if (chargingProfile->getChargingProfilePurpose() == ChargingProfilePurposeType::ChargePointMaxProfile) {
        if (connectorId > 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Cannot set ChargePointMaxProfile at connectorId > 0";
            return;
        }
    }

    accepted = scService.setChargingProfile(connectorId, std::move(chargingProfile));
}

std::unique_ptr<JsonDoc> SetChargingProfile::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (accepted) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
