// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/UuidUtils.h>

#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

namespace MicroOcpp {
    unsigned int g_randSeed = 1394827383;

    void writeRandomNonsecure(unsigned char *buf, size_t len) {
        g_randSeed += mocpp_tick_ms();
        const unsigned int a = 16807;
        const unsigned int m = 2147483647;
        for (size_t i = 0; i < len; i++) {
            g_randSeed = (a * g_randSeed) % m;
            buf[i] = g_randSeed;
        }
    }
}

using namespace MicroOcpp;

Request::Request(std::unique_ptr<Operation> msg) : MemoryManaged("Request.", msg->getOperationType()), messageID(makeString(getMemoryTag())), operation(std::move(msg)) {
    timeout_start = mocpp_tick_ms();
    debugRequest_start = mocpp_tick_ms();
}

Request::~Request(){

}

Operation *Request::getOperation(){
    return operation.get();
}

void Request::setTimeout(unsigned long timeout) {
    this->timeout_period = timeout;
}

bool Request::isTimeoutExceeded() {
    return timed_out || (timeout_period && mocpp_tick_ms() - timeout_start >= timeout_period);
}

void Request::executeTimeout() {
    if (!timed_out) {
        onTimeoutListener();
        onAbortListener();
    }
    timed_out = true;
}

void Request::setMessageID(const char *id){
    if (!messageID.empty()){
        MO_DBG_ERR("messageID already defined");
    }
    messageID = id;
}

Request::CreateRequestResult Request::createRequest(JsonDoc& requestJson) {

    if (messageID.empty()) {
        char uuid [37] = {'\0'};
        generateUUID(uuid, 37);
        messageID = uuid;
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
    size_t json_buffsize = JSON_ARRAY_SIZE(4) + (messageID.length() + 1) + requestPayload->capacity();
    requestJson = initJsonDoc(getMemoryTag(), json_buffsize);

    requestJson.add(MESSAGE_TYPE_CALL);                    //MessageType
    requestJson.add(messageID);                      //Unique message ID
    requestJson.add(operation->getOperationType());  //Action
    requestJson.add(*requestPayload);                      //Payload

    if (MO_DBG_LEVEL >= MO_DL_DEBUG && mocpp_tick_ms() - debugRequest_start >= 10000) { //print contents on the console
        debugRequest_start = mocpp_tick_ms();

        char *buf = new char[1024];
        size_t len = 0;
        if (buf) {
            len = serializeJson(requestJson, buf, 1024);
        }

        if (!buf || len < 1) {
            MO_DBG_DEBUG("Try to send request: %s", operation->getOperationType());
        } else {
            MO_DBG_DEBUG("Try to send request: %.*s (...)", 128, buf);
        }

        delete[] buf;
    }

    return CreateRequestResult::Success;
}

bool Request::receiveResponse(JsonArray response){

    /*
     * Validate array size to prevent NULL pointer dereference
     */
    if (response.size() < 2) {
        MO_DBG_ERR("malformed response: insufficient elements (size=%zu, expected>=2)", response.size());
        return false;
    }

    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (!response[1].is<const char*>() || messageID.compare(response[1].as<const char*>())) {
        return false;
    }

    int messageTypeId = response[0] | -1;

    if (messageTypeId == MESSAGE_TYPE_CALLRESULT) {

        // CALLRESULT format: [3, messageId, payload] - requires 3 elements
        if (response.size() < 3) {
            MO_DBG_ERR("malformed CALLRESULT: insufficient elements (size=%zu, expected>=3)", response.size());
            return false;
        }

        /*
        * Hand the payload over to the Operation object
        */
        JsonObject payload = response[2];
        operation->processConf(payload);

        /*
        * Hand the payload over to the onReceiveConf Callback
        */
        onReceiveConfListener(payload);

        /*
        * return true as this message has been consumed
        */
        return true;
    } else if (messageTypeId == MESSAGE_TYPE_CALLERROR) {

        // CALLERROR format: [4, messageId, errorCode, errorDescription, errorDetails] - requires 5 elements
        if (response.size() < 5) {
            MO_DBG_ERR("malformed CALLERROR: insufficient elements (size=%zu, expected>=5)", response.size());
            return false;
        }

        /*
        * Hand the error over to the Operation object
        */
        const char *errorCode = response[2];
        const char *errorDescription = response[3];
        JsonObject errorDetails = response[4];
        bool abortOperation = operation->processErr(errorCode, errorDescription, errorDetails);

        if (abortOperation) {
            onReceiveErrorListener(errorCode, errorDescription, errorDetails);
            onAbortListener();
        }

        return abortOperation;
    } else {
        MO_DBG_WARN("invalid response");
        return false; //don't discard this message but retry sending it
    }

}

bool Request::receiveRequest(JsonArray request) {

    // CALL format: [2, messageId, action, payload] - requires 4 elements
    if (request.size() < 4) {
        MO_DBG_ERR("malformed request: insufficient elements (size=%zu, expected>=4)", request.size());
        return false;
    }

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
    onReceiveReqListener(payload);

    return true; //success
}

Request::CreateResponseResult Request::createResponse(JsonDoc& response) {

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
        response.add(messageID.c_str());            //Unique message ID
        response.add(*payload);              //Payload

        if (onSendConfListener) {
            onSendConfListener(payload->as<JsonObject>());
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
        response.add(messageID.c_str());            //Unique message ID
        response.add(errorCode);
        response.add(errorDescription);
        response.add(*errorDetails);              //Error description
    }

    return CreateResponseResult::Success;
}

void Request::setOnReceiveConfListener(OnReceiveConfListener onReceiveConf){
    if (onReceiveConf)
        onReceiveConfListener = onReceiveConf;
}

/**
 * Sets a Listener that is called after this machine processed a request by the communication counterpart
 */
void Request::setOnReceiveReqListener(OnReceiveReqListener onReceiveReq){
    if (onReceiveReq)
        onReceiveReqListener = onReceiveReq;
}

void Request::setOnSendConfListener(OnSendConfListener onSendConf){
    if (onSendConf)
        onSendConfListener = onSendConf;
}

void Request::setOnTimeoutListener(OnTimeoutListener onTimeout) {
    if (onTimeout)
        onTimeoutListener = onTimeout;
}

void Request::setOnReceiveErrorListener(OnReceiveErrorListener onReceiveError) {
    if (onReceiveError)
        onReceiveErrorListener = onReceiveError;
}

void Request::setOnAbortListener(OnAbortListener onAbort) {
    if (onAbort)
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

std::unique_ptr<Request> makeRequest(std::unique_ptr<Operation> operation){
    if (operation == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<Request>(new Request(std::move(operation)));
}

std::unique_ptr<Request> makeRequest(Operation *operation) {
    return makeRequest(std::unique_ptr<Operation>(operation));
}

} //end namespace MicroOcpp
