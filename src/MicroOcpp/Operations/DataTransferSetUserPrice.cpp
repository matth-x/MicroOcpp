// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DataTransferSetUserPrice.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/California/CaliforniaService.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>
#include <ArduinoJson.hpp>

#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA


using namespace MicroOcpp;

v16::DataTransferSetUserPrice::DataTransferSetUserPrice(Model& model) : 
    MemoryManaged("v16.Operation.", "DataTransferSetUserPrice"), model(model)
{
}

v16::DataTransferSetUserPrice::~DataTransferSetUserPrice() {
}

const char* v16::DataTransferSetUserPrice::getOperationType(){
    return "DataTransferSetUserPrice";
}

void v16::DataTransferSetUserPrice::processReq(JsonObject payload) {
    if (!model.getCaliforniaService()->getCustomDisplayCostAndPrice()) {
        return;
    }
    
    if (!payload["data"].is<const char*>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("data not string");
        return;
    }

    // parse data field
    size_t size = 256;
    DynamicJsonDocument doc(size);
    DeserializationError err = DeserializationError::NoMemory;
    while (err == DeserializationError::NoMemory && size < MO_MAX_JSON_CAPACITY) {
        doc.clear();
        err = deserializeJson(doc, payload["data"].as<const char*>());
        size *= 2;
    }
    if (err != DeserializationError::Ok) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("Failed to parse RunningCost data: %s", err.c_str());
        return;
    }
    JsonObject data = doc.as<JsonObject>();
    
    const char* idToken;
    if (!data.containsKey("idToken")) {
        MO_DBG_DEBUG("Missing idToken");
        errorCode = "FormationViolation";
        return;
    }

    idToken = data["idToken"] | "Invalid";

    TransactionServiceEvse *transactionServiceEvse = nullptr;
    CaliforniaServiceEvse  *californiaServiceEvse = nullptr;
    for (uint32_t evseId = 0; evseId < MO_NUM_EVSEID; evseId++)
    {
        transactionServiceEvse = model.getTransactionService()->getEvse(evseId);
        if (!transactionServiceEvse || !transactionServiceEvse->getTransaction())
            continue;

        if (strcmp(idToken, transactionServiceEvse->getTransaction()->getIdTag()) == 0) {
            californiaServiceEvse = model.getCaliforniaService()->getEvse(evseId);
            break;
        }
    }

    if (!californiaServiceEvse || !transactionServiceEvse) {
        MO_DBG_ERR("idToken not found");
        errorCode = "InternalError";
        processed = false;
        return;
    }

    if (!data.containsKey("priceText")) {
        MO_DBG_DEBUG("Missing priceText");
        errorCode = "FormationViolation";
        return;
    }

    californiaServiceEvse->updateUserPriceText(data["priceText"]);
    processed = true;
}

std::unique_ptr<JsonDoc> v16::DataTransferSetUserPrice::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = processed ? "Accepted" : "Rejected";
    return doc;
}

#endif //MO_ENABLE_CALIFORNIA
#endif //MO_ENABLE_V16
