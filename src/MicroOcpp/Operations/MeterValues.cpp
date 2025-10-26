// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/MeterValues.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

//can only be used for echo server debugging
MeterValues::MeterValues(Context& context) : MemoryManaged("v16.Operation.", "MeterValues"), context(context) {

}

MeterValues::MeterValues(Context& context, unsigned int connectorId, int transactionId, MeterValue *meterValue, bool transferOwnership) :
        MemoryManaged("v16.Operation.", "MeterValues"),
        context(context),
        meterValue(meterValue),
        isMeterValueOwner(transferOwnership),
        connectorId(connectorId),
        transactionId(transactionId) {

}

MeterValues::~MeterValues(){
    if (isMeterValueOwner) {
        delete meterValue;
        meterValue = nullptr;
        isMeterValueOwner = false;
    }
}

const char* MeterValues::getOperationType(){
    return "MeterValues";
}

std::unique_ptr<JsonDoc> MeterValues::createReq() {

    size_t capacity = 0;
    capacity += JSON_OBJECT_SIZE(3);
    capacity += JSON_ARRAY_SIZE(1);

    int ret = meterValue->getJsonCapacity(MO_OCPP_V16, /*internalFormat*/ false);
    if (ret >= 0) {
        capacity += (size_t)ret;
    } else {
        MO_DBG_ERR("serialization error");
    }

    auto doc = makeJsonDoc(getMemoryTag(), capacity);
    auto payload = doc->to<JsonObject>();
    payload["connectorId"] = connectorId;

    if (transactionId >= 0) {
        payload["transactionId"] = transactionId;
    }

    auto meterValueArray = payload.createNestedArray("meterValue");
    auto meterValueJson = meterValueArray.createNestedObject();
    if (!meterValue->toJson(context.getClock(), MO_OCPP_V16, /*internalFormat*/ false, meterValueJson)) {
        MO_DBG_ERR("serialization error");
    }

    return doc;
}

void MeterValues::processConf(JsonObject payload) {
    MO_DBG_DEBUG("Request has been confirmed");
}


void MeterValues::processReq(JsonObject payload) {

    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */

}

std::unique_ptr<JsonDoc> MeterValues::createConf(){
    return createEmptyDocument();
}

#endif //MO_ENABLE_V16
