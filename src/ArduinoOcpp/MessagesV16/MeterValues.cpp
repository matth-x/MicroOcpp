// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>

#include <Variants.h>

using ArduinoOcpp::Ocpp16::MeterValues;

//can only be used for echo server debugging
MeterValues::MeterValues() {
    sampleTime = std::vector<OcppTimestamp>(); //not used in server mode but needed to keep the destructor simple
    energy = std::vector<float>();
    power = std::vector<float>();
}

MeterValues::MeterValues(std::vector<OcppTimestamp> *sampleTime, std::vector<float> *energy, std::vector<float> *power, int connectorId, int transactionId) 
      : connectorId(connectorId), transactionId(transactionId) {
    if (sampleTime)
        this->sampleTime = std::vector<OcppTimestamp>(*sampleTime);
    else
        this->sampleTime = std::vector<OcppTimestamp>();
    if (energy)
        this->energy = std::vector<float>(*energy);
    else
        this->energy = std::vector<float>();
    if (power)
        this->power = std::vector<float>(*power);
    else
        this->power = std::vector<float>();
}

MeterValues::~MeterValues(){

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
    for (size_t i = 0; i < sampleTime.size(); i++) {
        JsonObject meterValue = meterValues.createNestedObject();
        char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
        OcppTimestamp otimestamp = sampleTime.at(i);
        otimestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
        meterValue["timestamp"] = timestamp;
        JsonArray sampledValue = meterValue.createNestedArray("sampledValue");
        if (energy.size() >= i + 1) {
            JsonObject sampledValue_1 = sampledValue.createNestedObject();
            sampledValue_1["value"] = energy.at(i);
            sampledValue_1["measurand"] = "Energy.Active.Import.Register";
            sampledValue_1["unit"] = "Wh";
        }
        if (power.size() >= i + 1) {
            JsonObject sampledValue_2 = sampledValue.createNestedObject();
            sampledValue_2["value"] = power.at(i);
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
    //JsonObject payload = doc->to<JsonObject>();
    /*
     * empty payload
     */
    return doc;
}
