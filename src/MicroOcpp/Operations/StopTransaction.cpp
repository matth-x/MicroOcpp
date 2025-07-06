// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

StopTransaction::StopTransaction(Context& context, Transaction *transaction)
        : MemoryManaged("v16.Operation.", "StopTransaction"), context(context), transaction(transaction) {

}

const char* StopTransaction::getOperationType() {
    return "StopTransaction";
}

std::unique_ptr<JsonDoc> StopTransaction::createReq() {

    size_t capacity =
            JSON_OBJECT_SIZE(6) + //total of 6 fields
            MO_JSONDATE_SIZE; //timestamp string

    auto& txMeterValues = transaction->getTxMeterValues();
    capacity += JSON_ARRAY_SIZE(txMeterValues.size());
    for (size_t i = 0; i < txMeterValues.size(); i++) {
        int ret = txMeterValues[i]->getJsonCapacity(MO_OCPP_V16, /*internalFormat*/ false);
        if (ret >= 0) {
            capacity += (size_t)ret;
        } else {
            MO_DBG_ERR("tx meter values error");
        }
    }

    auto doc = makeJsonDoc(getMemoryTag(), capacity);
    JsonObject payload = doc->to<JsonObject>();

    if (transaction->getStopIdTag() && *transaction->getStopIdTag()) {
        payload["idTag"] = transaction->getStopIdTag();
    }

    payload["meterStop"] = transaction->getMeterStop();

    char timestamp [MO_JSONDATE_SIZE];
    if (!context.getClock().toJsonString(transaction->getStopTimestamp(), timestamp, sizeof(timestamp))) {
        MO_DBG_ERR("internal error");
    }
    payload["timestamp"] = timestamp;

    payload["transactionId"] = transaction->getTransactionId();

    if (transaction->getStopReason() && *transaction->getStopReason()) {
        payload["reason"] = transaction->getStopReason();
    }

    if (!txMeterValues.empty()) {
        JsonArray txMeterValuesJson = payload.createNestedArray("transactionData");

        for (size_t i = 0; i < txMeterValues.size(); i++) {
            auto mv = txMeterValues[i];
            auto mvJson = txMeterValuesJson.createNestedObject();

            if (mv->getJsonCapacity(MO_OCPP_V16, /*internalFormat*/ false) < 0) {
                //measurement has failed - ensure that serialization won't be attempted
                continue;
            }

            if (!mv->toJson(context.getClock(), MO_OCPP_V16, /*internal format*/ false, mvJson)) {
                MO_DBG_ERR("serialization error");
            }
        }
    }

    return doc;
}

void StopTransaction::processConf(JsonObject payload) {

    transaction->getStopSync().confirm();

    MO_DBG_INFO("Request has been accepted!");

#if MO_ENABLE_LOCAL_AUTH
    if (auto authService = context.getModel16().getAuthorizationService()) {
        authService->notifyAuthorization(transaction->getIdTag(), payload["idTagInfo"]);
    }
#endif //MO_ENABLE_LOCAL_AUTH
}

#if MO_ENABLE_MOCK_SERVER
int StopTransaction::writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
    (void)userStatus;
    (void)userData;
    return snprintf(buf, size, "{\"idTagInfo\":{\"status\":\"Accepted\"}}");
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V16
