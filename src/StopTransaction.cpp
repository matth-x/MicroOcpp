// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "StopTransaction.h"
#include "OcppEngine.h"
#include "MeteringService.h"


StopTransaction::StopTransaction() {

}

const char* StopTransaction::getOcppOperationType(){
    return "StopTransaction";
}

DynamicJsonDocument* StopTransaction::createReq() {

  String idTag = String('\0');

  if (getChargePointStatusService() != NULL) {
    idTag += getChargePointStatusService()->getIdTag();
  }

  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + (idTag.length() + 1) + (JSONDATE_LENGTH + 1));
  JsonObject payload = doc->to<JsonObject>();
  
  if (!idTag.isEmpty()) { //if there is no idTag present, we shouldn't add a default one
    payload["idTag"] = idTag;
  }

  float meterStop = 0.0f;
  if (getMeteringService() != NULL) {
    meterStop = getMeteringService()->readEnergyActiveImportRegister();
  }

  payload["meterStop"] = meterStop; //TODO meterStart is required to be in Wh, but measuring unit is probably inconsistent in implementation
  char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
  getJsonDateStringFromSystemTime(timestamp, JSONDATE_LENGTH);
  payload["timestamp"] = timestamp;

  int transactionId = -1;
  if (getChargePointStatusService() != NULL) {
    transactionId = getChargePointStatusService()->getTransactionId();
  }
  payload["transactionId"] = transactionId;

  if (getChargePointStatusService() != NULL) {
    getChargePointStatusService()->stopEnergyOffer();
  }

  return doc;
}

void StopTransaction::processConf(JsonObject payload) {

  //no need to process anything here

  ChargePointStatusService *cpStatusService = getChargePointStatusService();
  if (cpStatusService != NULL){
    //cpStatusService->stopEnergyOffer(); //No. This should remain in createReq
    cpStatusService->stopTransaction();
    cpStatusService->unauthorize();
  }

  SmartChargingService *scService = getSmartChargingService();
  if (scService != NULL){
    scService->endChargingNow();
  }

  if (DEBUG_OUT) Serial.print(F("[StopTransaction] Request has been accepted!\n"));

}


void StopTransaction::processReq(JsonObject payload) {
  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */
}

DynamicJsonDocument* StopTransaction::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1));
  JsonObject payload = doc->to<JsonObject>();

  JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
  idTagInfo["status"] = "Accepted";

  return doc;
}
