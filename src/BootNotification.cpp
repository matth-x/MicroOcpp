// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "BootNotification.h"
#include "EVSE.h"
#include "OcppEngine.h"

#include <string.h>
#include "TimeHelper.h"


BootNotification::BootNotification() {
  
}

BootNotification::BootNotification(String &cpModel, String &cpVendor) {
    chargePointModel = String(cpModel);
    chargePointVendor = String(cpVendor);
}

BootNotification::BootNotification(String &cpModel, String &cpVendor, String &cpSerialNumber) {
    chargePointModel = String(cpModel);
    chargePointVendor = String(cpVendor);
    chargePointSerialNumber = String(cpSerialNumber);
}

BootNotification::BootNotification(String &cpModel, String &cpVendor, String &cpSerialNumber, String &fwVersion) {
    chargePointModel = String(cpModel);
    chargePointVendor = String(cpVendor);
    chargePointSerialNumber = String(cpSerialNumber);
    firmwareVersion = String(fwVersion);
}

const char* BootNotification::getOcppOperationType(){
    return "BootNotification";
}

DynamicJsonDocument* BootNotification::createReq() {

  DynamicJsonDocument *doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(4)
      + chargePointModel.length() + 1
      + chargePointVendor.length() + 1
      + chargePointSerialNumber.length() + 1
      + firmwareVersion.length() + 1);
  JsonObject payload = doc->to<JsonObject>();
  payload["chargePointModel"] = chargePointModel;
  payload["chargePointVendor"] = chargePointVendor;
  if (!chargePointSerialNumber.isEmpty()) {
    payload["chargePointSerialNumber"] = chargePointSerialNumber;
  }
  if (!firmwareVersion.isEmpty()) {
    payload["firmwareVersion"] = firmwareVersion;
  }
  return doc;
}

void BootNotification::processConf(JsonObject payload){
  const char* currentTime = payload["currentTime"] | "Invalid";
  if (strcmp(currentTime, "Invalid")) {
    if (!setTimeFromJsonDateString(currentTime)) {
      Serial.print(F("[BootNotification] Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
    }
  } else {
    Serial.print(F("[BootNotification] Error reading time string. Missing attribute currentTime of type string\n"));
  }
  
  //int interval = payload["interval"] | 86400; //not used in this implementation

  const char* status = payload["status"] | "Invalid";

  if (!strcmp(status, "Accepted")) {
    if (DEBUG_OUT) Serial.print(F("[BootNotification] Request has been accepted!\n"));
    if (getChargePointStatusService() != NULL) {
      getChargePointStatusService()->boot();
    }
  } else {
    Serial.print(F("[BootNotification] Request unsuccessful!\n"));
  }
}

void BootNotification::processReq(JsonObject payload){
    /*
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

DynamicJsonDocument* BootNotification::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(3) + (JSONDATE_LENGTH + 1));
  JsonObject payload = doc->to<JsonObject>();
  //char currentTime[JSONDATE_LENGTH + 1] = {'\0'};
  //getJsonDateStringFromSystemTime(currentTime, JSONDATE_LENGTH);
  //payload["currentTime"] =  currentTime; //currentTime
  payload["currentTime"] = "2019-11-01T11:59:55.123Z";
  payload["interval"] = 86400; //heartbeat send interval - not relevant for JSON variant of OCPP so send dummy value that likely won't break
  payload["status"] = "Accepted";
  return doc;
}
