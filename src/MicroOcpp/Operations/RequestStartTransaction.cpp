// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::RequestStartTransaction;
using MicroOcpp::MemJsonDoc;

RequestStartTransaction::RequestStartTransaction(TransactionService& txService) : AllocOverrider("v201.Operation.", getOperationType()), txService(txService) {
  
}

const char* RequestStartTransaction::getOperationType(){
    return "RequestStartTransaction";
}

void RequestStartTransaction::processReq(JsonObject payload) {

    int evseId = payload["evseId"] | 0;
    if (evseId < 0 || evseId >= MO_NUM_EVSE) {
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

    status = txService.requestStartTransaction(evseId, remoteStartId, idToken, transactionId);
}

std::unique_ptr<MemJsonDoc> RequestStartTransaction::createConf(){

    auto doc = makeMemJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
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

    if (*transactionId) {
        payload["transactionId"] = (const char*)transactionId; //force zero-copy mode
    }

    return doc;
}

#endif // MO_ENABLE_V201
