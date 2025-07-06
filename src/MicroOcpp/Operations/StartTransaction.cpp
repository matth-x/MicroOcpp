// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StartTransaction.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

StartTransaction::StartTransaction(Context& context, Transaction *transaction) : MemoryManaged("v16.Operation.", "StartTransaction"), context(context), transaction(transaction) {

}

StartTransaction::~StartTransaction() {

}

const char* StartTransaction::getOperationType() {
    return "StartTransaction";
}

std::unique_ptr<JsonDoc> StartTransaction::createReq() {

    auto doc = makeJsonDoc(getMemoryTag(),
                JSON_OBJECT_SIZE(6) +
                MO_JSONDATE_SIZE);

    JsonObject payload = doc->to<JsonObject>();

    payload["connectorId"] = transaction->getConnectorId();
    payload["idTag"] = transaction->getIdTag();
    payload["meterStart"] = transaction->getMeterStart();

    if (transaction->getReservationId() >= 0) {
        payload["reservationId"] = transaction->getReservationId();
    }

    char timestamp[MO_JSONDATE_SIZE] = {'\0'};
    if (!context.getClock().toJsonString(transaction->getStartTimestamp(), timestamp, sizeof(timestamp))) {
        MO_DBG_ERR("internal error");
        timestamp[0] = '\0';
    }
    payload["timestamp"] = timestamp;

    return doc;
}

void StartTransaction::processConf(JsonObject payload) {

    const char* idTagInfoStatus = payload["idTagInfo"]["status"] | "not specified";
    if (!strcmp(idTagInfoStatus, "Accepted")) {
        MO_DBG_INFO("Request has been accepted");
    } else {
        MO_DBG_INFO("Request has been denied. Reason: %s", idTagInfoStatus);
        transaction->setIdTagDeauthorized();
    }

    int transactionId = payload["transactionId"] | -1;
    transaction->setTransactionId(transactionId);

    if (payload["idTagInfo"].containsKey("parentIdTag"))
    {
        transaction->setParentIdTag(payload["idTagInfo"]["parentIdTag"]);
    }

    transaction->getStartSync().confirm();

    if (auto filesystem = context.getFilesystem()) {
        TransactionStore::store(filesystem, context, *transaction);
    }

#if MO_ENABLE_LOCAL_AUTH
    if (auto authService = context.getModel16().getAuthorizationService()) {
        authService->notifyAuthorization(transaction->getIdTag(), payload["idTagInfo"]);
    }
#endif //MO_ENABLE_LOCAL_AUTH
}

#if MO_ENABLE_MOCK_SERVER
int StartTransaction::writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
    (void)userStatus;
    (void)userData;
    static int uniqueTxId = 1000;
    return snprintf(buf, size, "{\"idTagInfo\":{\"status\":\"Accepted\"}, \"transactionId\":%i}", uniqueTxId++);
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V16
