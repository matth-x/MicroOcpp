// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::v201;

RequestStopTransaction::RequestStopTransaction(RemoteControlService& rcService) : MemoryManaged("v201.Operation.", "RequestStopTransaction"), rcService(rcService) {
  
}

const char* RequestStopTransaction::getOperationType(){
    return "RequestStopTransaction";
}

void RequestStopTransaction::processReq(JsonObject payload) {

    if (!payload.containsKey("transactionId") || 
            !payload["transactionId"].is<const char*>() ||
            strlen(payload["transactionId"].as<const char*>()) + 1 > MO_TXID_SIZE) {
        errorCode = "FormationViolation";
        return;
    }

    status = rcService.requestStopTransaction(payload["transactionId"].as<const char*>());
}

std::unique_ptr<JsonDoc> RequestStopTransaction::createConf(){

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
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

    return doc;
}

#endif //MO_ENABLE_V201
