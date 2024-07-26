// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

namespace MicroOcpp {

//helper function
const char *cstrFromOcppEveState(ChargePointStatus state) {
    switch (state) {
        case (ChargePointStatus_Available):
            return "Available";
        case (ChargePointStatus_Preparing):
            return "Preparing";
        case (ChargePointStatus_Charging):
            return "Charging";
        case (ChargePointStatus_SuspendedEVSE):
            return "SuspendedEVSE";
        case (ChargePointStatus_SuspendedEV):
            return "SuspendedEV";
        case (ChargePointStatus_Finishing):
            return "Finishing";
        case (ChargePointStatus_Reserved):
            return "Reserved";
        case (ChargePointStatus_Unavailable):
            return "Unavailable";
        case (ChargePointStatus_Faulted):
            return "Faulted";
#if MO_ENABLE_V201
        case (ChargePointStatus_Occupied):
            return "Occupied";
#endif
        default:
            MO_DBG_ERR("ChargePointStatus not specified");
            /* fall through */
        case (ChargePointStatus_UNDEFINED):
            return "UNDEFINED";
    }
}

namespace Ocpp16 {

StatusNotification::StatusNotification(int connectorId, ChargePointStatus currentStatus, const Timestamp &timestamp, ErrorData errorData)
        : connectorId(connectorId), currentStatus(currentStatus), timestamp(timestamp), errorData(errorData) {
    
    if (currentStatus != ChargePointStatus_UNDEFINED) {
        MO_DBG_INFO("New status: %s (connectorId %d)", cstrFromOcppEveState(currentStatus), connectorId);
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
    } else if (currentStatus == ChargePointStatus_UNDEFINED) {
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

} // namespace Ocpp16
} // namespace MicroOcpp

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace Ocpp201 {

StatusNotification::StatusNotification(EvseId evseId, ChargePointStatus currentStatus, const Timestamp &timestamp)
        : evseId(evseId), timestamp(timestamp), currentStatus(currentStatus) {

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
    payload["evseId"] = evseId.id;
    payload["connectorId"] = evseId.id == 0 ? 0 : evseId.connectorId >= 0 ? evseId.connectorId : 1;

    return doc;
}


void StatusNotification::processConf(JsonObject payload) {
    /*
    * Empty payload
    */
}

} // namespace Ocpp201
} // namespace MicroOcpp

#endif //MO_ENABLE_V201
