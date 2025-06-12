// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/SetChargingProfile.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

using namespace MicroOcpp;

SetChargingProfile::SetChargingProfile(Context& context, SmartChargingService& scService) : MemoryManaged("v16.Operation.", "SetChargingProfile"), context(context), scService(scService), ocppVersion(context.getOcppVersion()) {

}

SetChargingProfile::~SetChargingProfile() {

}

const char* SetChargingProfile::getOperationType(){
    return "SetChargingProfile";
}

void SetChargingProfile::processReq(JsonObject payload) {

    int evseId = -1;
    JsonObject chargingProfileJson;

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        evseId = payload["connectorId"] | -1;
        if (evseId < 0 || !payload.containsKey("csChargingProfiles")) {
            errorCode = "FormationViolation";
            return;
        }

        if ((unsigned int) evseId >= context.getModel16().getNumEvseId()) {
            errorCode = "PropertyConstraintViolation";
            return;
        }

        chargingProfileJson = payload["csChargingProfiles"];
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        evseId = payload["evseId"] | -1;
        if (evseId < 0 || !payload.containsKey("chargingProfile")) {
            errorCode = "FormationViolation";
            return;
        }

        if ((unsigned int) evseId >= context.getModel201().getNumEvseId()) {
            errorCode = "PropertyConstraintViolation";
            return;
        }

        chargingProfileJson = payload["chargingProfile"];
    }
    #endif //MO_ENABLE_V201

    auto chargingProfile = std::unique_ptr<ChargingProfile>(new ChargingProfile());
    if (!chargingProfile) {
        MO_DBG_ERR("OOM");
        errorCode = "InternalError";
        return;
    }

    bool valid = chargingProfile->parseJson(context.getClock(), ocppVersion, chargingProfileJson);
    if (!valid) {
        errorCode = "FormationViolation";
        errorDescription = "chargingProfile validation failed";
        return;
    }

    if (chargingProfile->chargingProfilePurpose == ChargingProfilePurposeType::TxProfile) {
        // if TxProfile, check if a transaction is running

        if (evseId == 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Cannot set TxProfile at connectorId 0";
            return;
        }

        #if MO_ENABLE_V16
        if (ocppVersion == MO_OCPP_V16) {
            auto txService = context.getModel16().getTransactionService();
            auto txServiceEvse = txService ? txService->getEvse(evseId) : nullptr;
            if (!txServiceEvse) {
                errorCode = "InternalError";
                return;
            }

            auto transaction = txServiceEvse->getTransaction();
            if (!transaction || !transaction->isRunning()) {
                //no transaction running, reject profile
                accepted = false;
                return;
            }

            if (chargingProfile->transactionId16 >= 0 &&
                    chargingProfile->transactionId16 != transaction->getTransactionId()) {
                //transactionId undefined / mismatch
                accepted = false;
                return;
            }
        }
        #endif //MO_ENABLE_V16
        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            auto txService = context.getModel201().getTransactionService();
            auto txServiceEvse = txService ? txService->getEvse(evseId) : nullptr;
            if (!txServiceEvse) {
                errorCode = "InternalError";
                return;
            }

            auto transaction = txServiceEvse->getTransaction();
            if (!transaction || !(transaction->started && !transaction->stopped)) {
                //no transaction running, reject profile
                accepted = false;
                return;
            }

            if (*chargingProfile->transactionId201 >= 0 &&
                    strcmp(chargingProfile->transactionId201, transaction->transactionId)) {
                //transactionId undefined / mismatch
                accepted = false;
                return;
            }
        }
        #endif //MO_ENABLE_V201

        //seems good
    } else if (chargingProfile->chargingProfilePurpose == ChargingProfilePurposeType::ChargePointMaxProfile) {
        if (evseId > 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Cannot set ChargePointMaxProfile at connectorId > 0";
            return;
        }
    }

    accepted = scService.setChargingProfile(evseId, std::move(chargingProfile));
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

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
