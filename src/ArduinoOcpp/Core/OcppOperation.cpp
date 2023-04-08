// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Core/OperationStore.h>

#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>

#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

int unique_id_counter = 1000000;

using namespace ArduinoOcpp;

OcppOperation::OcppOperation(std::unique_ptr<OcppMessage> msg) : ocppMessage(std::move(msg)) {

}

OcppOperation::OcppOperation() {

}

OcppOperation::~OcppOperation(){

}

void OcppOperation::setOcppMessage(std::unique_ptr<OcppMessage> msg){
    ocppMessage = std::move(msg);
}

void OcppOperation::setTimeout(unsigned long timeout) {
    this->timeout_period = timeout;
}

bool OcppOperation::isTimeoutExceeded() {
    return timed_out || (timeout_period && ao_tick_ms() - timeout_start >= timeout_period);
}

void OcppOperation::executeTimeout() {
    if (!timed_out) {
        onTimeoutListener();
        onAbortListener();
    }
    timed_out = true;
}

unsigned int OcppOperation::getTrialNo() {
    return trialNo;
}

void OcppOperation::setMessageID(const std::string &id){
    if (!messageID.empty()){
        AO_DBG_WARN("MessageID is set twice or is set after first usage!");
    }
    messageID = id;
}

const char *OcppOperation::getMessageID() {
    return messageID.c_str();
}

std::unique_ptr<DynamicJsonDocument> OcppOperation::createRequest(){

    /*
     * Create the OCPP message
     */
    auto requestPayload = ocppMessage->createReq();
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
    requestJson->add(ocppMessage->getOcppOperationType());  //Action
    requestJson->add(*requestPayload);                      //Payload

    if (AO_DBG_LEVEL >= AO_DL_DEBUG && ao_tick_ms() - debugRequest_start >= 10000) { //print contents on the console
        debugRequest_start = ao_tick_ms();

        char *buf = new char[1024];
        size_t len = 0;
        if (buf) {
            len = serializeJson(*requestJson, buf, 1024);
        }

        if (!buf || len < 1) {
            AO_DBG_DEBUG("Try to send request: %s", ocppMessage->getOcppOperationType());
        } else {
            AO_DBG_DEBUG("Try to send request: %.*s ...", 128, ocppMessage->getOcppOperationType());
        }

        delete buf;
    }

    trialNo++;

    return requestJson;
}

bool OcppOperation::receiveResponse(JsonArray response){
    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (messageID != response[1].as<std::string>()){
        return false;
    }

    int messageTypeId = response[0] | -1;

    if (messageTypeId == MESSAGE_TYPE_CALLRESULT) {

        /*
        * Hand the payload over to the OcppMessage object
        */
        JsonObject payload = response[2];
        ocppMessage->processConf(payload);

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
        * Hand the error over to the OcppMessage object
        */
        const char *errorCode = response[2];
        const char *errorDescription = response[3];
        JsonObject errorDetails = response[4];
        bool abortOperation = ocppMessage->processErr(errorCode, errorDescription, errorDetails);

        if (abortOperation) {
            onReceiveErrorListener(errorCode, errorDescription, errorDetails);
            onAbortListener();
        }

        return abortOperation;
    } else {
        AO_DBG_WARN("invalid response");
        return false; //don't discard this message but retry sending it
    }

}

bool OcppOperation::receiveRequest(JsonArray request){
  
    std::string msgId = request[1];
    setMessageID(msgId);
    
    /*
     * Hand the payload over to the OcppOperation object
     */
    JsonObject payload = request[3];
    ocppMessage->processReq(payload);
    
    /*
     * Hand the payload over to the first Callback. It is a callback that notifies the client that request has been processed in the OCPP-library
     */
    onReceiveReqListener(payload);

    return true; //success
}

std::unique_ptr<DynamicJsonDocument> OcppOperation::createResponse(){

    /*
     * Create the OCPP message
     */
    std::unique_ptr<DynamicJsonDocument> response = nullptr;
    std::unique_ptr<DynamicJsonDocument> payload = ocppMessage->createConf();
    std::unique_ptr<DynamicJsonDocument> errorDetails = nullptr;
    
    bool operationFailure = ocppMessage->getErrorCode() != nullptr;

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
    } else {
        //operation failure. Send error message instead

        const char *errorCode = ocppMessage->getErrorCode();
        const char *errorDescription = ocppMessage->getErrorDescription();
        errorDetails = std::unique_ptr<DynamicJsonDocument>(ocppMessage->getErrorDetails());
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

void OcppOperation::initiate(std::unique_ptr<StoredOperationHandler> opStorage) {

    timeout_start = ao_tick_ms();
    debugRequest_start = ao_tick_ms();

    //assign messageID
    char id_str [16] = {'\0'};
    sprintf(id_str, "%d", unique_id_counter++);
    messageID = std::string {id_str};

    if (ocppMessage) {

        /*
         * Create OCPP-J Remote Procedure Call header storage entry
         */
        opStore = std::move(opStorage);

        if (opStore) {
            size_t json_buffsize = JSON_ARRAY_SIZE(3) + (messageID.length() + 1);
            auto rpcData = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

            rpcData->add(MESSAGE_TYPE_CALL);                    //MessageType
            rpcData->add(messageID);                      //Unique message ID
            rpcData->add(ocppMessage->getOcppOperationType());  //Action

            opStore->setRpc(std::move(rpcData));
        }
        
        ocppMessage->initiate(opStore.get());

        if (opStore) {
            opStore->clearBuffer();
        }
    } else {
        AO_DBG_ERR("Missing ocppMessage instance");
    }
}

bool OcppOperation::restore(std::unique_ptr<StoredOperationHandler> opStorage, std::shared_ptr<OcppModel> oModel) {
    if (!opStorage) {
        AO_DBG_ERR("invalid argument");
        return false;
    }

    opStore = std::move(opStorage);

    auto rpcData = opStore->getRpc();
    if (!rpcData) {
        AO_DBG_ERR("corrupted storage");
        return false;
    }

    messageID = (*rpcData)[1] | std::string();
    std::string opType = (*rpcData)[2] | std::string();
    if (messageID.empty() || opType.empty()) {
        AO_DBG_ERR("corrupted storage");
        messageID.clear();
        return false;
    }

    int parsedMessageID = -1;
    if (sscanf(messageID.c_str(), "%d", &parsedMessageID) == 1) {
        if (parsedMessageID > unique_id_counter) {
            AO_DBG_DEBUG("restore unique_id_counter with %d", parsedMessageID);
            unique_id_counter = parsedMessageID + 1; //next unique value is parsedId + 1
        }
    } else {
        AO_DBG_ERR("cannot set unique msgID counter");
        (void)0;
        //skip this step but don't abort restore
    }

    timeout_period = 0; //disable timeout by default for restored msgs

    if (!strcmp(opType.c_str(), "StartTransaction")) { //TODO this will get a nicer solution
        ocppMessage = std::unique_ptr<OcppMessage>(new Ocpp16::StartTransaction(*oModel.get(), nullptr));
    } else if (!strcmp(opType.c_str(), "StopTransaction")) {
        ocppMessage = std::unique_ptr<OcppMessage>(new Ocpp16::StopTransaction(*oModel.get(), nullptr));
    }

    if (!ocppMessage) {
        AO_DBG_ERR("cannot create msg");
        return false;
    }

    bool success = ocppMessage->restore(opStore.get());
    opStore->clearBuffer();

    if (success) {
        AO_DBG_DEBUG("restored opNr %i: %s", opStore->getOpNr(), ocppMessage->getOcppOperationType());
        (void)0;
    } else {
        AO_DBG_ERR("restore opNr %i error", opStore->getOpNr());
        (void)0;
    }

    return success;
}

void OcppOperation::setOnReceiveConfListener(OnReceiveConfListener onReceiveConf){
    if (onReceiveConf)
        onReceiveConfListener = onReceiveConf;
}

/**
 * Sets a Listener that is called after this machine processed a request by the communication counterpart
 */
void OcppOperation::setOnReceiveReqListener(OnReceiveReqListener onReceiveReq){
    if (onReceiveReq)
        onReceiveReqListener = onReceiveReq;
}

void OcppOperation::setOnSendConfListener(OnSendConfListener onSendConf){
    if (onSendConf)
        onSendConfListener = onSendConf;
}

void OcppOperation::setOnTimeoutListener(OnTimeoutListener onTimeout) {
    if (onTimeout)
        onTimeoutListener = onTimeout;
}

void OcppOperation::setOnReceiveErrorListener(OnReceiveErrorListener onReceiveError) {
    if (onReceiveError)
        onReceiveErrorListener = onReceiveError;
}

void OcppOperation::setOnAbortListener(OnAbortListener onAbort) {
    if (onAbort)
        onAbortListener = onAbort;
}

const char *OcppOperation::getOcppOperationType() {
    return ocppMessage ? ocppMessage->getOcppOperationType() : "UNDEFINED";
}
