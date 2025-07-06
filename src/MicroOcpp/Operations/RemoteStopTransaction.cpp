// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/RemoteStopTransaction.h>

#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

RemoteStopTransaction::RemoteStopTransaction(Context& context, RemoteControlService& rcService) : MemoryManaged("v16.Operation.", "RemoteStopTransaction"), context(context), rcService(rcService) {

}

const char* RemoteStopTransaction::getOperationType() {
    return "RemoteStopTransaction";
}

void RemoteStopTransaction::processReq(JsonObject payload) {

    if (!payload.containsKey("transactionId") || !payload["transactionId"].is<int>()) {
        errorCode = "FormationViolation";
    }
    int transactionId = payload["transactionId"];

    status = rcService.remoteStopTransaction(transactionId);
    if (status == v16::RemoteStartStopStatus::ERR_INTERNAL) {
        errorCode = "InternalError";
        return;
    }
}

std::unique_ptr<JsonDoc> RemoteStopTransaction::createConf() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    if (status == v16::RemoteStartStopStatus::Accepted) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}

#endif //MO_ENABLE_V16
