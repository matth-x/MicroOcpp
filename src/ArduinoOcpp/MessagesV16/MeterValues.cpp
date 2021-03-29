// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>

#include <Variants.h>

using ArduinoOcpp::Ocpp16::MeterValues;

//can only be used for echo server debugging
MeterValues::MeterValues() {
  sampleTime = LinkedList<time_t>(); //not used in server mode but needed to keep the destructor simple
  energy = LinkedList<float>();
  power = LinkedList<float>();
}

//MeterValues::MeterValues(LinkedList<time_t> *sample_t, LinkedList<float> *energyRegister) {
//  sampleTime = LinkedList<time_t>();
//  energy = LinkedList<float>();
//  power = LinkedList<float>(); //remains empty
//  for (int i = 0; i < energyRegister->size(); i++) {
//    sampleTime.add(sample_t->get(i));
//    energy.add((float) energyRegister->get(i));
//  }
//}

MeterValues::MeterValues(LinkedList<time_t> *sample_t, LinkedList<float> *energyRegister, LinkedList<float> *pwr, int connectorId, int transactionId) 
      : connectorId(connectorId), transactionId(transactionId) {
  sampleTime = LinkedList<time_t>();
  energy = LinkedList<float>();
  power = LinkedList<float>();
  for (int i = 0; i < sample_t->size(); i++)
    sampleTime.add(sample_t->get(i));
  if (energyRegister != NULL) {
    for (int i = 0; i < energyRegister->size(); i++)
      energy.add(energyRegister->get(i));
  }
  if (pwr != NULL) {
    for (int i = 0; i < pwr->size(); i++)
      power.add(pwr->get(i));
  }
}

MeterValues::~MeterValues(){
  sampleTime.clear();
  energy.clear();
  power.clear();
}

const char* MeterValues::getOcppOperationType(){
    return "MeterValues";
}

DynamicJsonDocument* MeterValues::createReq() {

  int numEntries = sampleTime.size();

  DynamicJsonDocument *doc = new DynamicJsonDocument(
      JSON_OBJECT_SIZE(2) //connectorID, transactionId
      + JSON_ARRAY_SIZE(numEntries) //metervalue array
      + numEntries * JSON_OBJECT_SIZE(1) //sampledValue
      + numEntries * (JSON_OBJECT_SIZE(1) + (JSONDATE_LENGTH + 1)) //timestamp
      + numEntries * JSON_ARRAY_SIZE(2) //sampledValue
      + 2 * numEntries * JSON_OBJECT_SIZE(1) //value          //   why are these taken by two?
      + 2 * numEntries * JSON_OBJECT_SIZE(1) //measurand      //
      + 2 * numEntries * JSON_OBJECT_SIZE(1) //unit           //
      + 230); //"safety space"
  JsonObject payload = doc->to<JsonObject>();
  
  payload["connectorId"] = connectorId;
  JsonArray meterValues = payload.createNestedArray("meterValue");
  for (int i = 0; i < sampleTime.size(); i++) {
    JsonObject meterValue = meterValues.createNestedObject();
    char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
    getJsonDateStringFromGivenTime(timestamp, JSONDATE_LENGTH + 1, sampleTime.get(i));
    meterValue["timestamp"] = timestamp;
    JsonArray sampledValue = meterValue.createNestedArray("sampledValue");
    if (energy.size() - 1 >= i) {
      JsonObject sampledValue_1 = sampledValue.createNestedObject();
      sampledValue_1["value"] = energy.get(i);
      sampledValue_1["measurand"] = "Energy.Active.Import.Register";
      sampledValue_1["unit"] = "Wh";
    }
    if (power.size() - 1 >= i) {
      JsonObject sampledValue_2 = sampledValue.createNestedObject();
      sampledValue_2["value"] = power.get(i);
      sampledValue_2["measurand"] = "Power.Active.Import";
      sampledValue_2["unit"] = "W";
    }
  }

  if (transactionId >= 0) {
    payload["transactionId"] = transactionId;
  }

  return doc;
}

void MeterValues::processConf(JsonObject payload) {
  if (DEBUG_OUT) Serial.print(F("[MeterValues] Request has been confirmed!\n"));
}


void MeterValues::processReq(JsonObject payload) {

  /**
   * Ignore Contents of this Req-message, because this is for debug purposes only
   */

}

DynamicJsonDocument* MeterValues::createConf(){
  DynamicJsonDocument* doc = new DynamicJsonDocument(0);
  JsonObject payload = doc->to<JsonObject>();
   /*
    * empty payload
    */
  return doc;
}
