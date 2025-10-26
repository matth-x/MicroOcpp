// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/RequestStartTransaction.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::v201;

RequestStartTransaction::RequestStartTransaction(Context& context, RemoteControlService& rcService) : MemoryManaged("v201.Operation.", "RequestStartTransaction"), context(context), rcService(rcService) {

}

const char* RequestStartTransaction::getOperationType(){
    return "RequestStartTransaction";
}

void RequestStartTransaction::processReq(JsonObject payload) {

    int evseId = payload["evseId"] | 0;
    if (evseId < 0 || evseId >= MO_NUM_EVSEID) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    int remoteStartId = payload["remoteStartId"] | 0;
    if (remoteStartId < 0) {
        errorCode = "PropertyConstraintViolation";
        MO_DBG_ERR("IDs must be >= 0");
        return;
    }

    IdToken idToken;
    if (!idToken.parseCstr(payload["idToken"]["idToken"] | (const char*)nullptr, payload["idToken"]["type"] | (const char*)nullptr)) { //parseCstr rejects nullptr as argument
        MO_DBG_ERR("could not parse idToken");
        errorCode = "FormationViolation";
        return;
    }

    std::unique_ptr<ChargingProfile> chargingProfile;
    if (payload.containsKey("chargingProfile")) {
        #if MO_ENABLE_SMARTCHARGING
        if (context.getModel201().getSmartChargingService()) {
            JsonObject chargingProfileJson = payload["chargingProfile"];
            chargingProfile = std::unique_ptr<ChargingProfile>(new ChargingProfile());
            if (!chargingProfile) {
                MO_DBG_ERR("OOM");
                errorCode = "InternalError";
                return;
            }

            bool valid = chargingProfile->parseJson(context.getClock(), MO_OCPP_V201, chargingProfileJson);
            if (!valid) {
                errorCode = "FormationViolation";
                return;
            }
        } else
        #endif
        {
            MO_DBG_INFO("ignore ChargingProfile"); //see F01.FR.12
        }
    }

    status = rcService.requestStartTransaction(evseId, remoteStartId, idToken, std::move(chargingProfile), transactionId, sizeof(transactionId));
}

std::unique_ptr<JsonDoc> RequestStartTransaction::createConf(){

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();

    const char *statusCstr = "";

    switch (status) {
        case RequestStartStopStatus::Accepted:
            statusCstr = "Accepted";
            break;
        case RequestStartStopStatus::Rejected:
            statusCstr = "Rejected";
            break;
        default:
            MO_DBG_ERR("internal error");
            break;
    }

    payload["status"] = statusCstr;

    if (transaction) {
        payload["transactionId"] = (const char*)transaction->transactionId; //force zero-copy mode
    }

    return doc;
}

#endif //MO_ENABLE_V201
