// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License


#include <ArduinoOcpp/MessagesV16/RemoteStartTransaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::RemoteStartTransaction;

RemoteStartTransaction::RemoteStartTransaction() {
  
}

const char* RemoteStartTransaction::getOcppOperationType() {
    return "RemoteStartTransaction";
}

void RemoteStartTransaction::processReq(JsonObject payload) {
    connectorId = payload["connectorId"] | -1;

    const char *idTagIn = payload["idTag"] | "";
    size_t len = strnlen(idTagIn, IDTAG_LEN_MAX + 2);
    if (len <= IDTAG_LEN_MAX) {
        snprintf(idTag, IDTAG_LEN_MAX + 1, "%s", idTagIn);
    }

    if (payload.containsKey("chargingProfile")) {
        AO_DBG_WARN("chargingProfile via RmtStartTransaction not supported yet");
    }
}

std::unique_ptr<DynamicJsonDocument> RemoteStartTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    if (*idTag == '\0') {
        AO_DBG_WARN("idTag format violation");
        payload["status"] = "Rejected";
        return doc;
    }
    
    bool canStartTransaction = false;
    if (connectorId >= 1) {
        //connectorId specified for given connector, try to start Transaction here
        if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
            auto connector = ocppModel->getConnectorStatus(connectorId);
            if (connector->getTransactionId() < 0 &&
                        connector->getAvailability() == AVAILABILITY_OPERATIVE &&
                        connector->getSessionIdTag() == nullptr) {
                canStartTransaction = true;
            }
        }
    } else {
        //connectorId not specified. Find free connector
        if (ocppModel && ocppModel->getChargePointStatusService()) {
            auto cpStatusService = ocppModel->getChargePointStatusService();
            for (int i = 1; i < cpStatusService->getNumConnectors(); i++) {
                auto connIter = cpStatusService->getConnector(i);
                if (connIter->getTransactionId() < 0 && 
                            connIter->getAvailability() == AVAILABILITY_OPERATIVE &&
                            connIter->getSessionIdTag() == nullptr) {
                    canStartTransaction = true;
                    connectorId = i;
                    break;
                }
            }
        }
    }

    if (canStartTransaction){
        if (ocppModel && ocppModel->getConnectorStatus(connectorId)) {
            auto connector = ocppModel->getConnectorStatus(connectorId);

            connector->beginSession(idTag);
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
