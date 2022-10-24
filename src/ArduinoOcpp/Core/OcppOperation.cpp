// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppModel.h>
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

void OcppOperation::setOcppModel(std::shared_ptr<OcppModel> oModel) {
    if (!ocppMessage) {
        AO_DBG_ERR("Must be called after setOcppMessage(). Abort");
        return;
    }
    if (!oModel) {
        AO_DBG_ERR("Passed nullptr. Ignore");
        return;
    }
    ocppMessage->setOcppModel(oModel);
}

void OcppOperation::setTimeout(std::unique_ptr<Timeout> to){
    if (!to){
        AO_DBG_ERR("Passed nullptr! Ignore");
        return;
    }
    timeout = std::move(to);
    timeout->setOnTimeoutListener(onTimeoutListener);
    timeout->setOnAbortListener(onAbortListener);
}

Timeout *OcppOperation::getTimeout() {
    return timeout.get();
}

void OcppOperation::setMessageID(const std::string &id){
    if (!messageID.empty()){
        AO_DBG_WARN("MessageID is set twice or is set after first usage!");
    }
    messageID = id;
}

const std::string *OcppOperation::getMessageID() {
    if (messageID.empty()) {
        char id_str [16] = {'\0'};
        sprintf(id_str, "%d", unique_id_counter++);
        messageID = std::string {id_str};
        //messageID = std::to_string(unique_id_counter++);
    }
    return &messageID;
}

bool OcppOperation::sendReq(OcppSocket& ocppSocket){

    /*
     * timeout behaviour
     * 
     * if timeout, print out error message and treat this operation as completed (-> return true)
     */
    if (timeout->isExceeded()) {
        //cancel this operation
        AO_DBG_INFO("%s has timed out! Discard operation", ocppMessage->getOcppOperationType());
        return true;
    }

    /*
     * retry behaviour
     * 
     * if retry, run the rest of this function, i.e. resend the message. If not, just return false
     */
    if (ao_tick_ms() <= retry_start + RETRY_INTERVAL * retry_interval_mult) {
        //NO retry
        return false;
    }
    // Do retry. Increase timer by factor 2
    if (RETRY_INTERVAL * retry_interval_mult * 2 <= RETRY_INTERVAL_MAX)
        retry_interval_mult *= 2;

    /*
     * Create the OCPP message
     */
    auto requestPayload = ocppMessage->createReq();
    if (!requestPayload) {
        return false;
    }

    /*
     * Create OCPP-J Remote Procedure Call header
     */
    size_t json_buffsize = JSON_ARRAY_SIZE(4) + (getMessageID()->length() + 1) + requestPayload->capacity();
    DynamicJsonDocument requestJson(json_buffsize);

    requestJson.add(MESSAGE_TYPE_CALL);                    //MessageType
    requestJson.add(*getMessageID());                      //Unique message ID
    requestJson.add(ocppMessage->getOcppOperationType());  //Action
    requestJson.add(*requestPayload);                      //Payload

    /*
     * Transaction safety: before sending the message, store the assigned msgId so this action
     * can be replayed when booted the next time
     */
    auto txSync = ocppMessage->getTransactionSync();
    if (txSync) {
        txSync->requestWithMsgId(unique_id_counter);
    }

    /*
     * Serialize and send. Destroy serialization and JSON object. 
     * 
     * If sending was successful, start timer
     * 
     * Return that this function must be called again (-> false)
     */
    std::string out {};
    serializeJson(requestJson, out);

    if (printReqCounter > 5000) {
        printReqCounter = 0;
        AO_DBG_DEBUG("Try to send request: %s", out.c_str());
    }
    printReqCounter++;
    
    bool success = ocppSocket.sendTXT(out);

    timeout->tick(success);
    
    if (success) {
        AO_DBG_TRAFFIC_OUT(out.c_str());
        retry_start = ao_tick_ms();
    } else {
        //ocppSocket is not able to put any data on TCP stack. Maybe because we're offline
        retry_start = 0;
        retry_interval_mult = 1;
    }

    return false;
}

bool OcppOperation::receiveConf(JsonDocument& confJson){
    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (*getMessageID() != confJson[1].as<std::string>()){
        return false;
    }

    /*
     * Hand the payload over to the OcppMessage object
     */
    JsonObject payload = confJson[2];
    ocppMessage->processConf(payload);

    /*
     * Hand the payload over to the onReceiveConf Callback
     */
    onReceiveConfListener(payload);

    /*
     * return true as this message has been consumed
     */
    return true;
}

bool OcppOperation::receiveError(JsonDocument& confJson){
    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (*getMessageID() != confJson[1].as<std::string>()){
        return false;
    }

    /*
     * Hand the error over to the OcppMessage object
     */
    const char *errorCode = confJson[2];
    const char *errorDescription = confJson[3];
    JsonObject errorDetails = confJson[4];
    bool abortOperation = ocppMessage->processErr(errorCode, errorDescription, errorDetails);

    if (abortOperation) {
        onReceiveErrorListener(errorCode, errorDescription, errorDetails);
        onAbortListener();
    } else {
        //restart operation
        timeout->restart();
        retry_start = 0;
        retry_interval_mult = 1;
    }

    return abortOperation;
}

bool OcppOperation::receiveReq(JsonDocument& reqJson){
  
    std::string reqId = reqJson[1];
    setMessageID(reqId);

    //TODO What if client receives requests two times? Can happen if previous conf is lost. In the Smart Charging Profile
    //     it probably doesn't matter to repeat an operation on the EVSE. Change in future?
    
    /*
     * Hand the payload over to the OcppOperation object
     */
    JsonObject payload = reqJson[3];
    ocppMessage->processReq(payload);
    
    /*
     * Hand the payload over to the first Callback. It is a callback that notifies the client that request has been processed in the OCPP-library
     */
    onReceiveReqListener(payload);

    reqExecuted = true; //ensure that the conf is only sent after the req has been executed

    return true; //true because everything was successful. If there will be an error check in future, this value becomes more reasonable
}

bool OcppOperation::sendConf(OcppSocket& ocppSocket){

    if (!reqExecuted) {
        //wait until req has been executed
        return false;
    }

    /*
     * Create the OCPP message
     */
    std::unique_ptr<DynamicJsonDocument> confJson = nullptr;
    std::unique_ptr<DynamicJsonDocument> confPayload = ocppMessage->createConf();
    std::unique_ptr<DynamicJsonDocument> errorDetails = nullptr;
    
    bool operationFailure = ocppMessage->getErrorCode() != nullptr;

    if (!operationFailure && !confPayload) {
        return false; //confirmation message still pending
    }

    if (!operationFailure) {

        /*
         * Create OCPP-J Remote Procedure Call header
         */
        size_t json_buffsize = JSON_ARRAY_SIZE(3) + (getMessageID()->length() + 1) + confPayload->capacity();
        confJson = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

        confJson->add(MESSAGE_TYPE_CALLRESULT);   //MessageType
        confJson->add(*getMessageID());            //Unique message ID
        confJson->add(*confPayload);              //Payload
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
                    + (getMessageID()->length() + 1)
                    + strlen(errorCode) + 1
                    + strlen(errorDescription) + 1
                    + errorDetails->capacity();
        confJson = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

        confJson->add(MESSAGE_TYPE_CALLERROR);   //MessageType
        confJson->add(*getMessageID());            //Unique message ID
        confJson->add(errorCode);
        confJson->add(errorDescription);
        confJson->add(*errorDetails);              //Error description
    }

    /*
     * Serialize and send. Destroy serialization and JSON object. 
     */
    std::string out {};
    serializeJson(*confJson, out);
    bool wsSuccess = ocppSocket.sendTXT(out);

    if (wsSuccess) {
        if (!operationFailure) {
            AO_DBG_TRAFFIC_OUT(out.c_str());
            onSendConfListener(confPayload->as<JsonObject>());
        } else {
            AO_DBG_WARN("Operation failed. JSON CallError message: %s", out.c_str());
            onAbortListener();
        }
    }

    return wsSuccess;
}

void OcppOperation::initiate(std::unique_ptr<StoredOperationHandler> opStorage) {
    if (ocppMessage) {

        /*
         * Create OCPP-J Remote Procedure Call header as storage data (doesn't necessarily have to comply with OCPP RPC header)
         */
        if (opStorage) {
            opStore = std::move(opStorage);
            size_t json_buffsize = JSON_ARRAY_SIZE(3) + (getMessageID()->length() + 1);
            auto rpcData = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(json_buffsize));

            rpcData->add(MESSAGE_TYPE_CALL);                    //MessageType
            rpcData->add(*getMessageID());                      //Unique message ID
            rpcData->add(ocppMessage->getOcppOperationType());  //Action

            opStore->setRpc(std::move(rpcData));
        }
        
        bool inited = ocppMessage->initiate(opStore.get());

        if (opStore) {
            opStore->releaseBuffer();
        }
        
        if (!inited) { //legacy support
            ocppMessage->initiate();
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
    auto payloadData = opStore->getPayload();
    if (!rpcData || !payloadData) {
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
            unique_id_counter = parsedMessageID;
            AO_DBG_VERBOSE("restore unique_id_counter = %d", unique_id_counter);
        }
    } else {
        AO_DBG_ERR("cannot set unique msgID counter");
        (void)0;
        //skip this step but don't abort restore
    }

    std::unique_ptr<OcppMessage> msg;

    if (!strcmp(opType.c_str(), "StartTransaction")) { //TODO this will get a nicer solution
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StartTransaction());
    } else if (!strcmp(opType.c_str(), "StopTransaction")) {
        msg = std::unique_ptr<OcppMessage>(new Ocpp16::StopTransaction());
    }

    if (!msg) {
        AO_DBG_ERR("cannot create msg");
        return false;
    }

    msg->setOcppModel(oModel);

    bool success = msg->restore(opStore.get());
    opStore->releaseBuffer();

    if (!success) {
        AO_DBG_ERR("restore error");
        (void)0;
    }

    return success;
}

void OcppOperation::finalize() {
    if (opStore) {
        opStore->confirm();
    }
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

bool OcppOperation::isFullyConfigured(){
    return ocppMessage != nullptr;
}

void OcppOperation::rebaseMsgId(int msgIdCounter) {
    unique_id_counter = msgIdCounter;
    getMessageID(); //apply msgIdCounter to this operation
}

void OcppOperation::print_debug() {
    if (ocppMessage) {
        AO_CONSOLE_PRINTF("OcppOperation of type %s\n", ocppMessage->getOcppOperationType());
    } else {
        AO_CONSOLE_PRINTF("OcppOperation (no type)\n");
    }
}
