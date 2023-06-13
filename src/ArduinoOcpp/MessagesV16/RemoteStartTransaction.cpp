// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License


#include <ArduinoOcpp/MessagesV16/RemoteStartTransaction.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Tasks/ChargeControl/Connector.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::RemoteStartTransaction;

RemoteStartTransaction::RemoteStartTransaction(Model& model) : model(model) {
  
}

const char* RemoteStartTransaction::getOperationType() {
    return "RemoteStartTransaction";
}

void RemoteStartTransaction::processReq(JsonObject payload) {
    connectorId = payload["connectorId"] | -1;

    const char *idTagIn = payload["idTag"] | "";
    size_t len = strnlen(idTagIn, IDTAG_LEN_MAX + 2);
    if (len <= IDTAG_LEN_MAX) {
        snprintf(idTag, IDTAG_LEN_MAX + 1, "%s", idTagIn);
    }

    if (*idTag == '\0') {
        AO_DBG_WARN("idTag format violation");
        errorCode = "FormationViolation";
    }

    if (payload.containsKey("chargingProfile")) {
        AO_DBG_INFO("Setting Charging profile via RemoteStartTransaction");

        JsonObject chargingProfile = payload["chargingProfile"];
        if ((chargingProfile["chargingProfileId"] | -1) < 0) {
            AO_DBG_WARN("RemoteStartTx profile requires non-negative chargingProfileId");
            errorCode = chargingProfile.containsKey("chargingProfileId") ? 
                        "PropertyConstraintViolation" : "FormationViolation";
        }
        chargingProfileDoc = DynamicJsonDocument(chargingProfile.memoryUsage()); //copy TxProfile
        chargingProfileDoc.set(chargingProfile);
    }
}

std::unique_ptr<DynamicJsonDocument> RemoteStartTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    
    bool canStartTransaction = false;
    if (connectorId >= 1) {
        //connectorId specified for given connector, try to start Transaction here
        if (auto connector = model.getConnector(connectorId)){
            if (connector->getTransactionId() < 0 &&
                        connector->getAvailability() == AVAILABILITY_OPERATIVE &&
                        connector->getSessionIdTag() == nullptr) {
                canStartTransaction = true;
            }
        }
    } else {
        //connectorId not specified. Find free connector
        for (unsigned int cid = 1; cid < model.getNumConnectors(); cid++) {
            auto connector = model.getConnector(cid);
            if (connector->getTransactionId() < 0 && 
                        connector->getAvailability() == AVAILABILITY_OPERATIVE &&
                        connector->getSessionIdTag() == nullptr) {
                canStartTransaction = true;
                connectorId = cid;
                break;
            }
        }
    }

    if (canStartTransaction) {

        auto sRmtProfileId = declareConfiguration<int>("AO_SRMTPROFILEID_CONN_1", -1, AO_KEYVALUE_FN, false, false, true, false);

        if (auto scService = model.getSmartChargingService()) {

            if (*sRmtProfileId >= 0) {
                int clearProfileId = *sRmtProfileId;
                bool ret = scService->clearChargingProfile([clearProfileId](int id, int, ChargingProfilePurposeType, int) {
                    return id == clearProfileId;
                });
                (void)ret;

                *sRmtProfileId = -1;
                AO_DBG_DEBUG("Cleared Charging Profile from previous RemoteStartTx: %s", ret ? "success" : "already cleared");
                configuration_save();
            }
        }

        if (auto connector = model.getConnector(connectorId)) {
            connector->beginTransaction_authorized(idTag);
        }

        if (!chargingProfileDoc.isNull()
                && (model.getSmartChargingService())) {
            auto scService = model.getSmartChargingService();

            JsonObject chargingProfile = chargingProfileDoc.as<JsonObject>();
            scService->setChargingProfile(chargingProfile);
            *sRmtProfileId = chargingProfile["chargingProfileId"].as<int>();
            AO_DBG_DEBUG("Charging Profile from RemoteStartTx set");
            configuration_save();
        }

        payload["status"] = "Accepted";
    } else {
        AO_DBG_INFO("No connector to start transaction");
        payload["status"] = "Rejected";
    }
    
    return doc;
}

std::unique_ptr<DynamicJsonDocument> RemoteStartTransaction::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    payload["idTag"] = "A0-00-00-00";

    return doc;
}

void RemoteStartTransaction::processConf(JsonObject payload){
    
}
