// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

using MicroOcpp::Ocpp16::StatusNotification;

//helper function
namespace MicroOcpp {
namespace Ocpp16 {
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
}} //end namespaces

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

//TODO if the status has changed again when sendReq() is called, abort the operation completely (note: if req is already sent, stick with listening to conf). The ChargePointStatusService will enqueue a new operation itself
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
