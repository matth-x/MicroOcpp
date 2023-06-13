// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Debug.h>

#include <string.h>

using ArduinoOcpp::Ocpp16::StatusNotification;

//helper function
namespace ArduinoOcpp {
namespace Ocpp16 {
const char *cstrFromOcppEveState(OcppEvseState state) {
    switch (state) {
        case (OcppEvseState::Available):
            return "Available";
        case (OcppEvseState::Preparing):
            return "Preparing";
        case (OcppEvseState::Charging):
            return "Charging";
        case (OcppEvseState::SuspendedEVSE):
            return "SuspendedEVSE";
        case (OcppEvseState::SuspendedEV):
            return "SuspendedEV";
        case (OcppEvseState::Finishing):
            return "Finishing";
        case (OcppEvseState::Reserved):
            return "Reserved";
        case (OcppEvseState::Unavailable):
            return "Unavailable";
        case (OcppEvseState::Faulted):
            return "Faulted";
        default:
            AO_DBG_ERR("OcppEvseState not specified");
            (void)0;
            /* fall through */
        case (OcppEvseState::NOT_SET):
            return "NOT_SET";
    }
}
}} //end namespaces

StatusNotification(int connectorId, OcppEvseState currentStatus, const Timestamp &timestamp, ErrorCode errorCode)
        : connectorId(connectorId), currentStatus(currentStatus), timestamp(timestamp), errorCode(errorCode) {
    
    AO_DBG_INFO("New status: %s (connectorId %d)", cstrFromOcppEveState(currentStatus), connectorId);
}

const char* StatusNotification::getOperationType(){
    return "StatusNotification";
}

//TODO if the status has changed again when sendReq() is called, abort the operation completely (note: if req is already sent, stick with listening to conf). The OcppEvseStateService will enqueue a new operation itself
std::unique_ptr<DynamicJsonDocument> StatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();
    
    payload["connectorId"] = connectorId;
    if (errorCode != nullptr) {
        payload["errorCode"] = errorCode;
    } else if (currentStatus == OcppEvseState::NOT_SET) {
        AO_DBG_ERR("Reporting undefined status");
        payload["errorCode"] = "InternalError";
    } else {
        payload["errorCode"] = "NoError";
    }

    payload["status"] = cstrFromOcppEveState(currentStatus);

    char timestamp_cstr[JSONDATE_LENGTH + 1] = {'\0'};
    timestamp.toJsonString(timestamp_cstr, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp_cstr;

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
void StatusNotification::processReq(JsonObject payload) {

}

/*
 * For debugging only
 */
std::unique_ptr<DynamicJsonDocument> StatusNotification::createConf(){
    return createEmptyDocument();
}
