// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::RequestStartTransaction;
using MicroOcpp::JsonDoc;

RequestStartTransaction::RequestStartTransaction(RemoteControlService& rcService) : MemoryManaged("v201.Operation.", "RequestStartTransaction"), rcService(rcService) {
  
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

    status = rcService.requestStartTransaction(evseId, remoteStartId, idToken, transactionId, sizeof(transactionId));
}

std::unique_ptr<JsonDoc> RequestStartTransaction::createConf(){

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
    JsonObject payload = doc->to<JsonObject>();

    const char *statusCstr = "";

    switch (status) {
        case RequestStartStopStatus_Accepted:
            statusCstr = "Accepted";
            break;
        case RequestStartStopStatus_Rejected:
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

#endif // MO_ENABLE_V201
