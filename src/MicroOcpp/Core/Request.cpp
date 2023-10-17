// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/RequestStore.h>

#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

int unique_id_counter = 1000000;

using namespace MicroOcpp;

Request::Request(std::unique_ptr<Operation> msg) : operation(std::move(msg)) {

}

Request::Request() {

}

Request::~Request(){

}

void Request::setOperation(std::unique_ptr<Operation> msg){
    operation = std::move(msg);
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

unsigned int Request::getTrialNo() {
    return trialNo;
}

void Request::setMessageID(const std::string &id){
    if (!messageID.empty()){
        MO_DBG_WARN("MessageID is set twice or is set after first usage!");
    }
    messageID = id;
}

const char *Request::getMessageID() {
    return messageID.c_str();
}

std::unique_ptr<DynamicJsonDocument> Request::createRequest(){

    /*
     * Create the OCPP message
     */
    auto requestPayload = operation->createReq();
    if (!requestPayload) {
        return nullptr;
    }

    /*
     * Create OCPP-J Remote Procedure Call header
     */
    size_t json_buffsize = JSON_ARRAY_SIZE(4) + (messageID.length() + 1) + requestPayload->capacity();
    auto requestJson = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

    requestJson->add(MESSAGE_TYPE_CALL);                    //MessageType
    requestJson->add(messageID);                      //Unique message ID
    requestJson->add(operation->getOperationType());  //Action
    requestJson->add(*requestPayload);                      //Payload

    if (MO_DBG_LEVEL >= MO_DL_DEBUG && mocpp_tick_ms() - debugRequest_start >= 10000) { //print contents on the console
        debugRequest_start = mocpp_tick_ms();

        char *buf = new char[1024];
        size_t len = 0;
        if (buf) {
            len = serializeJson(*requestJson, buf, 1024);
        }

        if (!buf || len < 1) {
            MO_DBG_DEBUG("Try to send request: %s", operation->getOperationType());
        } else {
            MO_DBG_DEBUG("Try to send request: %.*s (...)", 128, buf);
        }

        delete[] buf;
    }

    trialNo++;

    return requestJson;
}

bool Request::receiveResponse(JsonArray response){
    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (messageID != response[1].as<std::string>()){
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
        onReceiveConfListener(payload);

        /*
        * return true as this message has been consumed
        */
        return true;
    } else if (messageTypeId == MESSAGE_TYPE_CALLERROR) {

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

bool Request::receiveRequest(JsonArray request){
  
    std::string msgId = request[1];
    setMessageID(msgId);
    
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

std::unique_ptr<DynamicJsonDocument> Request::createResponse(){

    /*
     * Create the OCPP message
     */
    std::unique_ptr<DynamicJsonDocument> response = nullptr;
    std::unique_ptr<DynamicJsonDocument> payload = operation->createConf();
    std::unique_ptr<DynamicJsonDocument> errorDetails = nullptr;
    
    bool operationFailure = operation->getErrorCode() != nullptr;

    if (!operationFailure && !payload) {
        return nullptr; //confirmation message still pending
    }

    if (!operationFailure) {

        /*
         * Create OCPP-J Remote Procedure Call header
         */
        size_t json_buffsize = JSON_ARRAY_SIZE(3) + (messageID.length() + 1) + payload->capacity();
        response = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

        response->add(MESSAGE_TYPE_CALLRESULT);   //MessageType
        response->add(messageID);            //Unique message ID
        response->add(*payload);              //Payload

        if (onSendConfListener) {
            onSendConfListener(payload->as<JsonObject>());
        }
    } else {
        //operation failure. Send error message instead

        const char *errorCode = operation->getErrorCode();
        const char *errorDescription = operation->getErrorDescription();
        errorDetails = std::unique_ptr<DynamicJsonDocument>(operation->getErrorDetails());
        if (!errorCode) { //catch corner case when payload is null but errorCode is not set too!
            errorCode = "GenericError";
            errorDescription = "Could not create payload (createConf() returns Null)";
            errorDetails = std::unique_ptr<DynamicJsonDocument>(createEmptyDocument());
        }

        /*
         * Create OCPP-J Remote Procedure Call header
         */
        size_t json_buffsize = JSON_ARRAY_SIZE(5)
                    + (messageID.length() + 1)
                    + strlen(errorCode) + 1
                    + strlen(errorDescription) + 1
                    + errorDetails->capacity();
        response = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

        response->add(MESSAGE_TYPE_CALLERROR);   //MessageType
        response->add(messageID);            //Unique message ID
        response->add(errorCode);
        response->add(errorDescription);
        response->add(*errorDetails);              //Error description
    }

    return response;
}

void Request::initiate(std::unique_ptr<StoredOperationHandler> opStorage) {

    timeout_start = mocpp_tick_ms();
    debugRequest_start = mocpp_tick_ms();

    //assign messageID
    char id_str [16] = {'\0'};
    sprintf(id_str, "%d", unique_id_counter++);
    messageID = std::string {id_str};

    if (operation) {

        /*
         * Create OCPP-J Remote Procedure Call header storage entry
         */
        opStore = std::move(opStorage);

        if (opStore) {
            size_t json_buffsize = JSON_ARRAY_SIZE(3) + (messageID.length() + 1);
            auto rpcData = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

            rpcData->add(MESSAGE_TYPE_CALL);                    //MessageType
            rpcData->add(messageID);                      //Unique message ID
            rpcData->add(operation->getOperationType());  //Action

            opStore->setRpcData(std::move(rpcData));
        }
        
        operation->initiate(opStore.get());

        if (opStore) {
            opStore->clearBuffer();
        }
    } else {
        MO_DBG_ERR("Missing operation instance");
    }
}

bool Request::restore(std::unique_ptr<StoredOperationHandler> opStorage, Model *model) {
    if (!opStorage) {
        MO_DBG_ERR("invalid argument");
        return false;
    }

    opStore = std::move(opStorage);

    auto rpcData = opStore->getRpcData();
    if (!rpcData) {
        MO_DBG_ERR("corrupted storage");
        return false;
    }

    messageID = (*rpcData)[1] | std::string();
    std::string opType = (*rpcData)[2] | std::string();
    if (messageID.empty() || opType.empty()) {
        MO_DBG_ERR("corrupted storage");
        messageID.clear();
        return false;
    }

    int parsedMessageID = -1;
    if (sscanf(messageID.c_str(), "%d", &parsedMessageID) == 1) {
        if (parsedMessageID > unique_id_counter) {
            MO_DBG_DEBUG("restore unique_id_counter with %d", parsedMessageID);
            unique_id_counter = parsedMessageID + 1; //next unique value is parsedId + 1
        }
    } else {
        MO_DBG_ERR("cannot set unique msgID counter");
        (void)0;
        //skip this step but don't abort restore
    }

    timeout_period = 0; //disable timeout by default for restored msgs

    if (!strcmp(opType.c_str(), "StartTransaction") && model) { //TODO this will get a nicer solution
        operation = std::unique_ptr<Operation>(new Ocpp16::StartTransaction(*model, nullptr));
    } else if (!strcmp(opType.c_str(), "StopTransaction") && model) {
        operation = std::unique_ptr<Operation>(new Ocpp16::StopTransaction(*model, nullptr));
    }

    if (!operation) {
        MO_DBG_ERR("cannot create msg");
        return false;
    }

    bool success = operation->restore(opStore.get());
    opStore->clearBuffer();

    if (success) {
        MO_DBG_DEBUG("restored opNr %i: %s", opStore->getOpNr(), operation->getOperationType());
        (void)0;
    } else {
        MO_DBG_ERR("restore opNr %i error", opStore->getOpNr());
        (void)0;
    }

    return success;
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
