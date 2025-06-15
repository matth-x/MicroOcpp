// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/UuidUtils.h>

#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Request::Request(Context& context, std::unique_ptr<Operation> operation) : MemoryManaged("Request.", operation->getOperationType()), context(context), operation(std::move(operation)) {
    timeoutStart = context.getClock().getUptime();
    debugRequestTime = context.getClock().getUptime();
}

Request::~Request(){
    if (onAbortListener) {
        onAbortListener();
    }
    setMessageID(nullptr);
}

Operation *Request::getOperation(){
    return operation.get();
}

void Request::setTimeout(int32_t timeout) {
    this->timeoutPeriod = timeout;
}

bool Request::isTimeoutExceeded() {
    auto& clock = context.getClock();
    int32_t dt;
    clock.delta(clock.getUptime(), timeoutStart, dt);
    return timedOut || (timeoutPeriod > 0 && dt >= timeoutPeriod);
}

void Request::executeTimeout() {
    if (!timedOut) {
        if (onTimeoutListener) {
            onTimeoutListener();
            onTimeoutListener = nullptr;
        }
        if (onAbortListener) {
            onAbortListener();
            onAbortListener = nullptr;
        }
    }
    timedOut = true;
}

bool Request::setMessageID(const char *id){
    MO_FREE(messageID);
    messageID = nullptr;

    if (id) {
        size_t size = strlen(id) + 1;
        messageID = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
        if (!messageID) {
            MO_DBG_ERR("OOM");
            return false;
        }
        memset(messageID, 0, size);
        auto ret = snprintf(messageID, size, "%s", id);
        if (ret < 0 || (size_t)ret >= size) {
            MO_DBG_ERR("snprintf: %i", ret);
            MO_FREE(messageID);
            messageID = nullptr;
            return false;
        }
    }

    //success
    return true;
}

Request::CreateRequestResult Request::createRequest(JsonDoc& requestJson) {

    if (!messageID) {
        char uuid [MO_UUID_STR_SIZE] = {'\0'};
        UuidUtils::generateUUID(context.getRngCb(), uuid, sizeof(uuid));
        setMessageID(uuid);
    }

    if (!messageID) {
        return CreateRequestResult::Failure;
    }

    /*
     * Create the OCPP message
     */
    auto requestPayload = operation->createReq();
    if (!requestPayload) {
        return CreateRequestResult::Failure;
    }

    /*
     * Create OCPP-J Remote Procedure Call header
     */
    size_t json_buffsize = JSON_ARRAY_SIZE(4) + requestPayload->capacity();
    requestJson = initJsonDoc(getMemoryTag(), json_buffsize);

    requestJson.add(MESSAGE_TYPE_CALL);              //MessageType
    requestJson.add((const char*)messageID);         //Unique message ID (force zero-copy)
    requestJson.add(operation->getOperationType());  //Action
    requestJson.add(*requestPayload);                //Payload

    if (MO_DBG_LEVEL >= MO_DL_DEBUG) {
        auto& clock = context.getClock();
        int32_t dt;
        clock.delta(clock.now(), debugRequestTime, dt);
        if (dt >= 10) {
            debugRequestTime = clock.now();

            char *buf = static_cast<char*>(MO_MALLOC(getMemoryTag(), 1024));
            size_t len = 0;
            if (buf) {
                len = serializeJson(requestJson, buf, 1024);
            }

            if (!buf || len < 1) {
                MO_DBG_DEBUG("Try to send request: %s", operation->getOperationType());
            } else {
                MO_DBG_DEBUG("Try to send request: %.*s (...)", 128, buf);
            }

            MO_FREE(buf);
        }
    }

    return CreateRequestResult::Success;
}

bool Request::receiveResponse(JsonArray response){

    //check if messageIDs match
    if (!messageID || strcmp(messageID, response[1].as<const char*>())) {
        return false;
    }

    int messageTypeId = response[0] | -1;

    if (messageTypeId == MESSAGE_TYPE_CALLRESULT) {

        /*
        * Hand the payload over to the Operation object
        */
        JsonObject payload = response[2];
        operation->processConf(payload);

        /*
        * Hand the payload over to the onReceiveConf Callback
        */
        if (onReceiveConfListener) {
            onReceiveConfListener(payload);
        }

        onAbortListener = nullptr; //ensure onAbort is not called during destruction

    } else if (messageTypeId == MESSAGE_TYPE_CALLERROR) {

        /*
        * Hand the error over to the Operation object
        */
        const char *errorCode = response[2];
        const char *errorDescription = response[3];
        JsonObject errorDetails = response[4];
        operation->processErr(errorCode, errorDescription, errorDetails);

        if (onReceiveErrorListener) {
            onReceiveErrorListener(errorCode, errorDescription, errorDetails);
        }
        if (onAbortListener) {
            onAbortListener();
            onAbortListener = nullptr; //ensure onAbort is called only once
        }

    } else {
        MO_DBG_WARN("invalid response");
        return false; //don't discard this message but retry sending it
    }

    return true;
}

bool Request::receiveRequest(JsonArray request) {

    if (!request[1].is<const char*>()) {
        MO_DBG_ERR("malformatted msgId");
        return false;
    }
  
    setMessageID(request[1].as<const char*>());
    
    /*
     * Hand the payload over to the Request object
     */
    JsonObject payload = request[3];
    operation->processReq(payload);
    
    /*
     * Hand the payload over to the first Callback. It is a callback that notifies the client that request has been processed in the OCPP-library
     */
    if (onReceiveReqListener) {
        size_t bufsize = 1000; //fixed for now, may change to direct pass-through of the input message
        char *buf = static_cast<char*>(MO_MALLOC(getMemoryTag(), bufsize));
        if (buf) {
            auto written = serializeJson(payload, buf, bufsize);
            if (written >= 2 && written < bufsize) {
                //success
                onReceiveReqListener(operation->getOperationType(), buf, onReceiveReqListenerUserData);
            } else {
                MO_DBG_ERR("onReceiveReqListener supports only %zu charactes", bufsize);
            }
        } else {
            MO_DBG_ERR("OOM");
        }
        MO_FREE(buf);
    }

    return true; //success
}

Request::CreateResponseResult Request::createResponse(JsonDoc& response) {

    if (!messageID) {
        return CreateResponseResult::Failure;
    }

    bool operationFailure = operation->getErrorCode() != nullptr;

    if (!operationFailure) {

        std::unique_ptr<JsonDoc> payload = operation->createConf();

        if (!payload) {
            return CreateResponseResult::Pending; //confirmation message still pending
        }

        /*
         * Create OCPP-J Remote Procedure Call header
         */
        size_t json_buffsize = JSON_ARRAY_SIZE(3) + payload->capacity();
        response = initJsonDoc(getMemoryTag(), json_buffsize);

        response.add(MESSAGE_TYPE_CALLRESULT);   //MessageType
        response.add((const char*)messageID);    //Unique message ID (force zero-copy)
        response.add(*payload);                  //Payload

        if (onSendConfListener) {
            size_t bufsize = 1000; //fixed for now, may change to direct pass-through of the input message
            char *buf = static_cast<char*>(MO_MALLOC(getMemoryTag(), bufsize));
            if (buf) {
                auto written = serializeJson(payload->as<JsonObject>(), buf, bufsize);
                if (written >= 2 && written < bufsize) {
                    //success
                    onSendConfListener(operation->getOperationType(), buf, onSendConfListenerUserData);
                } else {
                    MO_DBG_ERR("onSendConfListener supports only %zu charactes", bufsize);
                }
            } else {
                MO_DBG_ERR("OOM");
            }
            MO_FREE(buf);
        }
    } else {
        //operation failure. Send error message instead

        const char *errorCode = operation->getErrorCode();
        const char *errorDescription = operation->getErrorDescription();
        std::unique_ptr<JsonDoc> errorDetails = operation->getErrorDetails();

        /*
         * Create OCPP-J Remote Procedure Call header
         */
        size_t json_buffsize = JSON_ARRAY_SIZE(5)
                    + errorDetails->capacity();
        response = initJsonDoc(getMemoryTag(), json_buffsize);

        response.add(MESSAGE_TYPE_CALLERROR);   //MessageType
        response.add((const char*)messageID);   //Unique message ID (force zero-copy)
        response.add(errorCode);
        response.add(errorDescription);
        response.add(*errorDetails);             //Error description
    }

    onAbortListener = nullptr; //ensure onAbort is not called during destruction

    return CreateResponseResult::Success;
}

void Request::setOnReceiveConf(ReceiveConfListener onReceiveConf){
    onReceiveConfListener = onReceiveConf;
}

/**
 * Sets a Listener that is called after this machine processed a request by the communication counterpart
 */
void Request::setOnReceiveRequest(void (*onReceiveReq)(const char *operationType, const char *payloadJson, void *userData), void *userData){
    this->onReceiveReqListener = onReceiveReq;
    this->onReceiveReqListenerUserData = userData;
}

void Request::setOnSendConf(void (*onSendConf)(const char *operationType, const char *payloadJson, void *userData), void *userData){
    this->onSendConfListener = onSendConf;
    this->onSendConfListenerUserData = userData;
}

void Request::setOnTimeout(TimeoutListener onTimeout) {
    onTimeoutListener = onTimeout;
}

void Request::setReceiveErrorListener(ReceiveErrorListener onReceiveError) {
    onReceiveErrorListener = onReceiveError;
}

void Request::setOnAbort(AbortListener onAbort) {
    onAbortListener = onAbort;
}

const char *Request::getOperationType() {
    return operation ? operation->getOperationType() : "UNDEFINED";
}

void Request::setRequestSent() {
    requestSent = true;
}

bool Request::isRequestSent() {
    return requestSent;
}

namespace MicroOcpp {

std::unique_ptr<Request> makeRequest(Context& context, std::unique_ptr<Operation> operation){
    if (operation == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<Request>(new Request(context, std::move(operation)));
}

std::unique_ptr<Request> makeRequest(Context& context, Operation *operation) {
    return makeRequest(context, std::unique_ptr<Operation>(operation));
}

} //namespace MicroOcpp
