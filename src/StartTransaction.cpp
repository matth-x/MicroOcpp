// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "StartTransaction.h"
#include "TimeHelper.h"
#include "OcppEngine.h"
#include "MeteringService.h"


StartTransaction::StartTransaction() {
    if (getChargePointStatusService() != NULL) {
      if (!getChargePointStatusService()->getIdTag().isEmpty()) {
        idTag = String(getChargePointStatusService()->getIdTag());
      }
    }
    if (idTag.isEmpty())
      idTag = String("fefed1d19876"); //Use a default payload. In the typical use case of this library, you probably you don't even need Authorization at all
}

StartTransaction::StartTransaction(String &idTag) {
    this->idTag = String(idTag);
}

const char* StartTransaction::getOcppOperationType(){
    return "StartTransaction";
}

DynamicJsonDocument* StartTransaction::createReq() {
  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(5) + (JSONDATE_LENGTH + 1) + (idTag.length() + 1));
  JsonObject payload = doc->to<JsonObject>();

  payload["connectorId"] = 1;
  MeteringService* meteringService = getMeteringService();
  if (meteringService != NULL) {
	  payload["meterStart"] = meteringService->readEnergyActiveImportRegister();
  }
  char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromGivenTime(timestamp, JSONDATE_LENGTH + 1, now());
  payload["timestamp"] = timestamp;
  payload["idTag"] = idTag;

  return doc;
}

void StartTransaction::processConf(JsonObject payload) {

  const char* idTagInfoStatus = payload["idTagInfo"]["status"] | "Invalid";
  int transactionId = payload["transactionId"] | -1;

  if (!strcmp(idTagInfoStatus, "Accepted")) {
    if (DEBUG_OUT) Serial.print(F("[StartTransaction] Request has been accepted!\n"));

    ChargePointStatusService *cpStatusService = getChargePointStatusService();
    if (cpStatusService != NULL){
      cpStatusService->startTransaction(transactionId);
      cpStatusService->startEnergyOffer();
    }

    SmartChargingService *scService = getSmartChargingService();
    if (scService != NULL){
      scService->beginChargingNow();
    }

  } else {
    Serial.print(F("[StartTransaction] Request has been denied!\n"));
  }
}


void StartTransaction::processReq(JsonObject payload) {

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */

}

DynamicJsonDocument* StartTransaction::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2));
  JsonObject payload = doc->to<JsonObject>();

  JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
  idTagInfo["status"] = "Accepted";
  payload["transactionId"] = 123456; //sample data for debug purpose

  return doc;
}
