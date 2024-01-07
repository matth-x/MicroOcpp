// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

namespace MicroOcpp {
namespace Ocpp16 {

//helper function
const char *cstrFromOcppEveState(ChargePointStatus state) {
    switch (state) {
        case (ChargePointStatus::Available):
            return "Available";
        case (ChargePointStatus::Preparing):
            return "Preparing";
        case (ChargePointStatus::Charging):
            return "Charging";
        case (ChargePointStatus::SuspendedEVSE):
            return "SuspendedEVSE";
        case (ChargePointStatus::SuspendedEV):
            return "SuspendedEV";
        case (ChargePointStatus::Finishing):
            return "Finishing";
        case (ChargePointStatus::Reserved):
            return "Reserved";
        case (ChargePointStatus::Unavailable):
            return "Unavailable";
        case (ChargePointStatus::Faulted):
            return "Faulted";
        default:
            MO_DBG_ERR("ChargePointStatus not specified");
            (void)0;
            /* fall through */
        case (ChargePointStatus::NOT_SET):
            return "NOT_SET";
    }
}

StatusNotification::StatusNotification(int connectorId, ChargePointStatus currentStatus, const Timestamp &timestamp, ErrorData errorData)
        : connectorId(connectorId), currentStatus(currentStatus), timestamp(timestamp), errorData(errorData) {
    
    if (currentStatus != ChargePointStatus::NOT_SET) {
        MO_DBG_INFO("New status: %s (connectorId %d)", cstrFromOcppEveState(currentStatus), connectorId);
        (void)0;
    }
}

const char* StatusNotification::getOperationType(){
    return "StatusNotification";
}

std::unique_ptr<DynamicJsonDocument> StatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(7) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();

    payload["connectorId"] = connectorId;
    if (errorData.isError) {
        if (errorData.errorCode) {
            payload["errorCode"] = errorData.errorCode;
        }
        if (errorData.info) {
            payload["info"] = errorData.info;
        }
        if (errorData.vendorId) {
            payload["vendorId"] = errorData.vendorId;
        }
        if (errorData.vendorErrorCode) {
            payload["vendorErrorCode"] = errorData.vendorErrorCode;
        }
    } else if (currentStatus == ChargePointStatus::NOT_SET) {
        MO_DBG_ERR("Reporting undefined status");
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

} //end namespace Ocpp16

#if MO_ENABLE_V201

namespace Ocpp201 {

const char *cstrFromOcppEveState(ConnectorStatus state) {
    switch (state) {
        case (ConnectorStatus::Available):
            return "Available";
        case (ConnectorStatus::Occupied):
            return "Occupied";
        case (ConnectorStatus::Reserved):
            return "Reserved";
        case (ConnectorStatus::Unavailable):
            return "Unavailable";
        case (ConnectorStatus::Faulted):
            return "Faulted";
        default:
            MO_DBG_ERR("ConnectorStatus not specified");
            (void)0;
            /* fall through */
        case (ConnectorStatus::NOT_SET):
            return "NOT_SET";
    }
}

StatusNotification::StatusNotification(int evseId, ConnectorStatus currentStatus, const Timestamp &timestamp, int connectorId)
        : evseId(evseId), currentStatus(currentStatus), timestamp(timestamp), connectorId(connectorId) {

}

const char* StatusNotification::getOperationType(){
    return "StatusNotification";
}

std::unique_ptr<DynamicJsonDocument> StatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();

    char timestamp_cstr[JSONDATE_LENGTH + 1] = {'\0'};
    timestamp.toJsonString(timestamp_cstr, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp_cstr;
    payload["connectorStatus"] = cstrFromOcppEveState(currentStatus);
    payload["evseId"] = connectorId;
    payload["connectorId"] = 1;

    return doc;
}


void StatusNotification::processConf(JsonObject payload) {
    /*
    * Empty payload
    */
}

} //end namespace Ocpp201

#endif //MO_ENABLE_V201

} //end namespace MicroOcpp
