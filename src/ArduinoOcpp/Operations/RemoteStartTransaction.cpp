// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License


#include <ArduinoOcpp/Operations/RemoteStartTransaction.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/ChargeControl/Connector.h>
#include <ArduinoOcpp/Model/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Model/Transactions/Transaction.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::RemoteStartTransaction;

RemoteStartTransaction::RemoteStartTransaction(Model& model) : model(model) {
  
}

const char* RemoteStartTransaction::getOperationType() {
    return "RemoteStartTransaction";
}

void RemoteStartTransaction::processReq(JsonObject payload) {
    connectorId = payload["connectorId"] | -1;

    if (!payload.containsKey("idTag")) {
        errorCode = "FormationViolation";
        return;
    }

    const char *idTagIn = payload["idTag"] | "";
    size_t len = strnlen(idTagIn, IDTAG_LEN_MAX + 1);
    if (len > 0 && len <= IDTAG_LEN_MAX) {
        snprintf(idTag, IDTAG_LEN_MAX + 1, "%s", idTagIn);
    } else {
        errorCode = "PropertyConstraintViolation";
        errorDescription = "idTag empty or too long";
        return;
    }

    if (payload.containsKey("chargingProfile") && model.getSmartChargingService()) {
        AO_DBG_INFO("Setting Charging profile via RemoteStartTransaction");

        JsonObject chargingProfileJson = payload["chargingProfile"];
        chargingProfile = loadChargingProfile(chargingProfileJson);

        if (!chargingProfile) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "chargingProfile validation failed";
            return;
        }

        if (chargingProfile->getChargingProfilePurpose() != ChargingProfilePurposeType::TxProfile) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Can only set TxProfile here";
            return;
        }

        if (chargingProfile->getChargingProfileId() < 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "RemoteStartTx profile requires non-negative chargingProfileId";
            return;
        }
    }
}

std::unique_ptr<DynamicJsonDocument> RemoteStartTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    
    Connector *selectConnector = nullptr;
    if (connectorId >= 1) {
        //connectorId specified for given connector, try to start Transaction here
        if (auto connector = model.getConnector(connectorId)){
            if (!connector->getTransaction() &&
                        connector->isOperative()) {
                selectConnector = connector;
            }
        }
    } else {
        //connectorId not specified. Find free connector
        for (unsigned int cid = 1; cid < model.getNumConnectors(); cid++) {
            auto connector = model.getConnector(cid);
            if (!connector->getTransaction() &&
                        connector->isOperative()) {
                selectConnector = connector;
                connectorId = cid;
                break;
            }
        }
    }

    if (selectConnector) {

        bool success = true;

        int chargingProfileId = -1; //keep Id after moving charging profile to SCService

        if (chargingProfile && model.getSmartChargingService()) {
            chargingProfileId = chargingProfile->getChargingProfileId();
            success = model.getSmartChargingService()->setChargingProfile(connectorId, std::move(chargingProfile));
        }

        if (success) {
            auto tx = selectConnector->beginTransaction_authorized(idTag);
            selectConnector->updateTxNotification(TxNotification::RemoteStart);
            if (tx) {
                if (chargingProfileId >= 0) {
                    tx->setTxProfileId(chargingProfileId);
                }
            } else {
                success = false;
            }
        }

        if (success) {
            payload["status"] = "Accepted";
        } else {
            payload["status"] = "Rejected";
        }
    } else {
        AO_DBG_INFO("No connector to start transaction");
        payload["status"] = "Rejected";
    }
    
    return doc;
}

std::unique_ptr<DynamicJsonDocument> RemoteStartTransaction::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    payload["idTag"] = "A0000000";

    return doc;
}

void RemoteStartTransaction::processConf(JsonObject payload){
    
}
