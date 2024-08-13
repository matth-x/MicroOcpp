// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::RequestStopTransaction;
using MicroOcpp::JsonDoc;

RequestStopTransaction::RequestStopTransaction(TransactionService& txService) : MemoryManaged("v201.Operation.", "RequestStopTransaction"), txService(txService) {
  
}

const char* RequestStopTransaction::getOperationType(){
    return "RequestStopTransaction";
}

void RequestStopTransaction::processReq(JsonObject payload) {

    if (!payload.containsKey("transactionId") || 
            !payload["transactionId"].is<const char*>() ||
            strlen(payload["transactionId"].as<const char*>()) > MO_TXID_LEN_MAX) {
        errorCode = "FormationViolation";
        return;
    }

    status = txService.requestStopTransaction(payload["transactionId"].as<const char*>());
}

std::unique_ptr<JsonDoc> RequestStopTransaction::createConf(){

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
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

    return doc;
}

#endif // MO_ENABLE_V201
