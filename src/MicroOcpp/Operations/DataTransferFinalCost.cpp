// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/California/CaliforniaService.h>
#include <MicroOcpp/Operations/DataTransferFinalCost.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA

using namespace MicroOcpp;

v16::DataTransferFinalCost::DataTransferFinalCost(Context& context) : 
    MemoryManaged("v16.Operation.", "DataTransferFinalCost"), context(context)
{
}

v16::DataTransferFinalCost::~DataTransferFinalCost() {
}

const char* v16::DataTransferFinalCost::getOperationType(){
    return "DataTransferFinalCost";
}

void v16::DataTransferFinalCost::processReq(JsonObject payload) {
    if (!payload["data"].is<const char*>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("data not string");
        return;
    }

    // parse data field
    size_t capacity = 512;
    DeserializationError err = DeserializationError::NoMemory;
    std::unique_ptr<MicroOcpp::JsonDoc> doc;
    while (err == DeserializationError::NoMemory && capacity < MO_MAX_JSON_CAPACITY) {
        doc = makeJsonDoc(getMemoryTag(), capacity);
        err = deserializeJson(*doc, payload["data"].as<const char*>());
        capacity *= 2;
    }

    if (err != DeserializationError::Ok) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("Failed to parse FinalCost data: %s", err.c_str());
        return;
    }

    auto data = doc->as<JsonObject>();
    if (!data.containsKey("transactionId") || !data.containsKey("cost") ||
            !data.containsKey("priceText") || !data.containsKey("qrCodeText")) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("FinalCost data missing required fields");
        return;
    }

    int transactionId = data["transactionId"].as<int>();

    auto& model = context.getModel16();
    TransactionServiceEvse *transactionServiceEvse = nullptr;
    CaliforniaServiceEvse  *californiaServiceEvse = nullptr;
    for (uint32_t evseId = 0; evseId < MO_NUM_EVSEID; evseId++) {
        transactionServiceEvse = model.getTransactionService()->getEvse(evseId);
        if (transactionServiceEvse && transactionServiceEvse->getTransaction() && 
            transactionServiceEvse->getTransaction()->getTransactionId() == transactionId) {
            californiaServiceEvse = model.getCaliforniaService()->getEvse(evseId);
            break;
        }
    }

    if (!californiaServiceEvse || !transactionServiceEvse) {
        errorCode = "InternalError";
        MO_DBG_DEBUG("transactionId not found");
        return;
    }

    if (!data["cost"].is<double>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("cost not double");
        return;
    }
    double cost = data["cost"].as<double>();

    if (!data["priceText"].is<const char*>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("priceText not string");
        return;
    }
    const char* priceText = data["priceText"].as<const char*>();

    if (!data["qrCodeText"].is<const char*>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("qrCodeText not string");
        return;
    }
    const char* qrCodeText = data["qrCodeText"].as<const char*>();

    californiaServiceEvse->updateCost(cost);
    californiaServiceEvse->updatePriceTextExtra(priceText);
    californiaServiceEvse->updateQrCodeText(qrCodeText);
}

std::unique_ptr<JsonDoc> v16::DataTransferFinalCost::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Rejected";
    return doc;
}

#endif //MO_ENABLE_CALIFORNIA
#endif //MO_ENABLE_V16
