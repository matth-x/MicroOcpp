// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>

#include <Variants.h>

using ArduinoOcpp::Ocpp16::StartTransaction;

StartTransaction::StartTransaction(int connectorId) : connectorId(connectorId) {
    this->idTag = String('\0');
}

StartTransaction::StartTransaction(int connectorId, String &idTag) : connectorId(connectorId) {
    this->idTag = String(idTag);
}

const char* StartTransaction::getOcppOperationType(){
    return "StartTransaction";
}

void StartTransaction::initiate() {
    if (ocppModel && ocppModel->getMeteringService()) {
        auto meteringService = ocppModel->getMeteringService();
        meterStart = (int) meteringService->readEnergyActiveImportRegister(connectorId);
    }

    if (ocppModel) {
        otimestamp = ocppModel->getOcppTime().getOcppTimestampNow();
    } else {
        otimestamp = MIN_TIME;
    }

    if (idTag.isEmpty()) {
        if (ocppModel && ocppModel->getChargePointStatusService() 
                    && ocppModel->getChargePointStatusService()->existsUnboundAuthorization()) {
            this->idTag = String(ocppModel->getChargePointStatusService()->getUnboundIdTag()); 
        } else {
            //The CP is not authorized. Try anyway, let the CS decide what to do ...
            this->idTag = String("A0-00-00-00"); //Use a default payload. In the typical use case of this library, you probably you don't even need Authorization at all
        }
    }

    if (ocppModel && ocppModel->getChargePointStatusService()) {
        ocppModel->getChargePointStatusService()->bindAuthorization(connectorId);
    }

    if (ocppModel && ocppModel->getConnectorStatus(connectorId)){
        auto connector = ocppModel->getConnectorStatus(connectorId);
        if (connector->getTransactionId() >= 0) {
            Serial.print(F("[StartTransaction] Warning: started transaction while OCPP already presumes a running transaction\n"));
        }
        connector->setTransactionId(0); //pending
        transactionRev = connector->getTransactionWriteCount();
    }

    if (DEBUG_OUT) Serial.println(F("[StartTransaction] StartTransaction initiated!"));
}

std::unique_ptr<DynamicJsonDocument> StartTransaction::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(5) + (JSONDATE_LENGTH + 1) + (idTag.length() + 1)));
    JsonObject payload = doc->to<JsonObject>();

    payload["connectorId"] = connectorId;
    if (meterStart >= 0) {
        payload["meterStart"] = meterStart;
    }

    if (otimestamp > MIN_TIME) {
        char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
        otimestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
        payload["timestamp"] = timestamp;
    }

    payload["idTag"] = idTag;

    return doc;
}

void StartTransaction::processConf(JsonObject payload) {

    const char* idTagInfoStatus = payload["idTagInfo"]["status"] | "not specified";
    int transactionId = payload["transactionId"] | -1;

    ConnectorStatus *connector = nullptr;
    if (ocppModel)
        connector = ocppModel->getConnectorStatus(connectorId);

    if (!strcmp(idTagInfoStatus, "Accepted")) {
        if (DEBUG_OUT) Serial.print(F("[StartTransaction] Request has been accepted!\n"));

        if (connector){
            if (transactionRev == connector->getTransactionWriteCount()) {
                connector->setTransactionId(transactionId);
            }
            connector->setTransactionIdSync(transactionId);
        }
    } else {
        Serial.print(F("[StartTransaction] Request has been denied! Reason: "));
        Serial.println(idTagInfoStatus);
        if (connector){
            if (transactionRev == connector->getTransactionWriteCount()) {
                connector->setTransactionId(-1);
                connector->unauthorize();
            }
            connector->setTransactionIdSync(-1);
        }
    }
}


void StartTransaction::processReq(JsonObject payload) {

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */

}

std::unique_ptr<DynamicJsonDocument> StartTransaction::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2)));
    JsonObject payload = doc->to<JsonObject>();

    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
    idTagInfo["status"] = "Accepted";
    payload["transactionId"] = 123456; //sample data for debug purpose

    return doc;
}
