// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/StatusNotification.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;

v16::StatusNotification::StatusNotification(Context& context, int connectorId, MO_ChargePointStatus currentStatus, const Timestamp &timestamp, MO_ErrorData errorData)
        : MemoryManaged("v16.Operation.", "StatusNotification"), context(context), connectorId(connectorId), currentStatus(currentStatus), timestamp(timestamp), errorData(errorData) {
    
    if (currentStatus != MO_ChargePointStatus_UNDEFINED) {
        MO_DBG_INFO("New status: %s (connectorId %d)", mo_serializeChargePointStatus(currentStatus), connectorId);
    }
}

const char* v16::StatusNotification::getOperationType(){
    return "StatusNotification";
}

std::unique_ptr<JsonDoc> v16::StatusNotification::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(7) + MO_JSONDATE_SIZE);
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
    } else if (currentStatus == MO_ChargePointStatus_UNDEFINED) {
        MO_DBG_ERR("Reporting undefined status");
        payload["errorCode"] = "InternalError";
    } else {
        payload["errorCode"] = "NoError";
    }

    payload["status"] = mo_serializeChargePointStatus(currentStatus);

    char timestampStr [MO_JSONDATE_SIZE] = {'\0'};
    if (!context.getClock().toJsonString(timestamp, timestampStr, sizeof(timestampStr))) {
        MO_DBG_ERR("internal error");
        timestampStr[0] = '\0';
    }
    payload["timestamp"] = timestampStr;

    return doc;
}

void v16::StatusNotification::processConf(JsonObject payload) {
    /*
    * Empty payload
    */
}

/*
 * For debugging only
 */
void v16::StatusNotification::processReq(JsonObject payload) {

}

/*
 * For debugging only
 */
std::unique_ptr<JsonDoc> v16::StatusNotification::createConf(){
    return createEmptyDocument();
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

using namespace MicroOcpp;

v201::StatusNotification::StatusNotification(Context& context, EvseId evseId, MO_ChargePointStatus currentStatus, const Timestamp &timestamp)
        : MemoryManaged("v201.Operation.", "StatusNotification"), context(context), evseId(evseId), timestamp(timestamp), currentStatus(currentStatus) {

}

const char* v201::StatusNotification::getOperationType(){
    return "StatusNotification";
}

std::unique_ptr<JsonDoc> v201::StatusNotification::createReq() {
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(4) + MO_JSONDATE_SIZE);
    JsonObject payload = doc->to<JsonObject>();

    char timestampStr [MO_JSONDATE_SIZE] = {'\0'};
    if (!context.getClock().toJsonString(timestamp, timestampStr, sizeof(timestampStr))) {
        MO_DBG_ERR("internal error");
        timestampStr[0] = '\0';
    }
    payload["timestamp"] = timestampStr;
    payload["connectorStatus"] = mo_serializeChargePointStatus(currentStatus);
    payload["evseId"] = evseId.id;
    payload["connectorId"] = evseId.id == 0 ? 0 : evseId.connectorId >= 0 ? evseId.connectorId : 1;

    return doc;
}


void v201::StatusNotification::processConf(JsonObject payload) {
    /*
    * Empty payload
    */
}

/*
 * For debugging only
 */
void v201::StatusNotification::processReq(JsonObject payload) {

}

/*
 * For debugging only
 */
std::unique_ptr<JsonDoc> v201::StatusNotification::createConf(){
    return createEmptyDocument();
}

#endif //MO_ENABLE_V201
