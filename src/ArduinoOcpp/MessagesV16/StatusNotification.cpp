// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <Variants.h>

#include <string.h>

using ArduinoOcpp::Ocpp16::StatusNotification;

StatusNotification::StatusNotification(int connectorId, OcppEvseState currentStatus, const OcppTimestamp &otimestamp, const char *errorCode) 
  : connectorId(connectorId), currentStatus(currentStatus), otimestamp(otimestamp), errorCode(errorCode) {

    if (DEBUG_OUT) {
        Serial.print(F("[StatusNotification] New StatusNotification. New Status: "));
    }

    switch (currentStatus) {
        case (OcppEvseState::Available):
            if (DEBUG_OUT) Serial.print(F("Available\n"));
            break;
        case (OcppEvseState::Preparing):
            if (DEBUG_OUT) Serial.print(F("Preparing\n"));
            break;
        case (OcppEvseState::Charging):
            if (DEBUG_OUT) Serial.print(F("Charging\n"));
            break;
        case (OcppEvseState::SuspendedEVSE):
            if (DEBUG_OUT) Serial.print(F("SuspendedEVSE\n"));
            break;
        case (OcppEvseState::SuspendedEV):
            if (DEBUG_OUT) Serial.print(F("SuspendedEV\n"));
            break;
        case (OcppEvseState::Finishing):
            if (DEBUG_OUT) Serial.print(F("Finishing\n"));
            break;
        case (OcppEvseState::Reserved):
            if (DEBUG_OUT) Serial.print(F("Reserved\n"));
            break;
        case (OcppEvseState::Unavailable):
            if (DEBUG_OUT) Serial.print(F("Unavailable\n"));
            break;
        case (OcppEvseState::Faulted):
            if (DEBUG_OUT) Serial.print(F("Faulted\n"));
            break;
        case (OcppEvseState::NOT_SET):
            Serial.print(F("NOT_SET\n"));
            break;
        default:
            Serial.print(F("[Error, invalid status code]\n"));
            break;
    }
}

const char* StatusNotification::getOcppOperationType(){
    return "StatusNotification";
}

//TODO if the status has changed again when sendReq() is called, abort the operation completely (note: if req is already sent, stick with listening to conf). The OcppEvseStateService will enqueue a new operation itself
std::unique_ptr<DynamicJsonDocument> StatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();
    
    payload["connectorId"] = connectorId;
    if (errorCode != nullptr) {
        payload["errorCode"] = errorCode;
    } else {
        payload["errorCode"] = "NoError";
    }
    
    switch (currentStatus) {
        case (OcppEvseState::Available):
            payload["status"] = "Available";
            break;
        case (OcppEvseState::Preparing):
            payload["status"] = "Preparing";
            break;
        case (OcppEvseState::Charging):
            payload["status"] = "Charging";
            break;
        case (OcppEvseState::SuspendedEVSE):
            payload["status"] = "SuspendedEVSE";
            break;
        case (OcppEvseState::SuspendedEV):
            payload["status"] = "SuspendedEV";
            break;
        case (OcppEvseState::Finishing):
            payload["status"] = "Finishing";
            break;
        case (OcppEvseState::Reserved):
            payload["status"] = "Reserved";
            break;
        case (OcppEvseState::Unavailable):
            payload["status"] = "Unavailable";
            break;
        case (OcppEvseState::Faulted):
            payload["status"] = "Faulted";
            break;
        default:
            payload["status"] = "NOT_SET";
            Serial.print(F("[StatusNotification] Error: Sending status NOT_SET!\n"));
            break;
    }

    char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
    otimestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp;

    return doc;
}


void StatusNotification::processConf(JsonObject payload) {
    /*
    * Empty payload
    */
}


/* 
 * For debugging only
 */
StatusNotification::StatusNotification() {
    
}

/*
 * For debugging only
 */
void StatusNotification::processReq(JsonObject payload) {

}

/*
 * For debugging only
 */
std::unique_ptr<DynamicJsonDocument> StatusNotification::createConf(){
    return createEmptyDocument();
}
