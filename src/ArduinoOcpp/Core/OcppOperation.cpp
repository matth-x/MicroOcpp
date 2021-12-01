// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppOperation.h>

#include <Variants.h>

int unique_id_counter = 531531531;

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

void OcppOperation::setTimeout(std::unique_ptr<Timeout> to){
    if (!to){
        Serial.print(F("[OcppOperation] in setTimeout(): passed nullptr! Ignore\n"));
        return;
    }
    timeout = std::move(to);
    timeout->setOnTimeoutListener(onTimeoutListener);
    timeout->setOnAbortListener(onAbortListener);
}

Timeout *OcppOperation::getTimeout() {
    return timeout.get();
}

void OcppOperation::setMessageID(const String &id){
    if (messageID.length() > 0){
        Serial.print(F("[OcppOperation] OcppOperation (class): MessageID is set twice or is set after first usage!\n"));
        //return; <-- Just letting the id being overwritten probably doesn't cause further errors...
    }
    messageID = id;
}

const String *OcppOperation::getMessageID() {
    if (messageID.isEmpty()) {
        messageID = String(unique_id_counter++);
    }
    return &messageID;
}

boolean OcppOperation::sendReq(OcppSocket& ocppSocket){

    /*
     * timeout behaviour
     * 
     * if timeout, print out error message and treat this operation as completed (-> return true)
     */
    if (timeout->isExceeded()) {
        //cancel this operation
        Serial.print(F("[OcppOperation] "));
        Serial.print(ocppMessage->getOcppOperationType());
        Serial.print(F(" has timed out! Discard operation\n"));
        return true;
    }

    /*
     * retry behaviour
     * 
     * if retry, run the rest of this function, i.e. resend the message. If not, just return false
     */
    if (millis() <= retry_start + RETRY_INTERVAL * retry_interval_mult) {
        //NO retry
        return false;
    }
    // Do retry. Increase timer by factor 2
    if (RETRY_INTERVAL * retry_interval_mult * 2 <= RETRY_INTERVAL_MAX)
        retry_interval_mult *= 2;

    /*
     * Create the OCPP message
     */
    auto requestPayload = std::unique_ptr<DynamicJsonDocument>(ocppMessage->createReq());
    if (!requestPayload) {
        onAbortListener();
        return true;
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
     * Serialize and send. Destroy serialization and JSON object. 
     * 
     * If sending was successful, start timer
     * 
     * Return that this function must be called again (-> false)
     */
    String out {'\0'};
    serializeJson(requestJson, out);

#if DEBUG_OUT
    if (printReqCounter > 5000) {
        printReqCounter = 0;
        Serial.print(F("[OcppOperation] Send requirement: "));
        Serial.print(out);
        Serial.print(F("\n"));
    }
    printReqCounter++;
#endif
    
    bool success = ocppSocket.sendTXT(out);

    timeout->tick(success);
    
    if (success) {
        if (TRAFFIC_OUT) Serial.print(F("[OcppOperation] Sent requirement (success): "));
        if (TRAFFIC_OUT) Serial.println(out);
        retry_start = millis();
    } else {
        //ocppSocket is not able to put any data on TCP stack. Maybe because we're offline
        retry_start = 0;
        retry_interval_mult = 1;
    }

    return false;
}

boolean OcppOperation::receiveConf(JsonDocument& confJson){
    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (!getMessageID()->equals(confJson[1].as<String>())){
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

boolean OcppOperation::receiveError(JsonDocument& confJson){
    /*
     * check if messageIDs match. If yes, continue with this function. If not, return false for message not consumed
     */
    if (!getMessageID()->equals(confJson[1].as<String>())){
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

boolean OcppOperation::receiveReq(JsonDocument& reqJson){
  
    String reqId = reqJson[1];
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

boolean OcppOperation::sendConf(OcppSocket& ocppSocket){

    if (!reqExecuted) {
        //wait until req has been executed
        return false;
    }

    /*
     * Create the OCPP message
     */
    std::unique_ptr<DynamicJsonDocument> confJson = nullptr;
    std::unique_ptr<DynamicJsonDocument> confPayload = std::unique_ptr<DynamicJsonDocument>(ocppMessage->createConf());
    std::unique_ptr<DynamicJsonDocument> errorDetails = nullptr;
    
    bool operationSuccess = ocppMessage->getErrorCode() == nullptr && confPayload != nullptr;

    if (operationSuccess) {

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
    String out {'\0'};
    serializeJson(*confJson, out);
    boolean wsSuccess = ocppSocket.sendTXT(out);

    if (wsSuccess) {
        if (operationSuccess) {
            if (DEBUG_OUT || TRAFFIC_OUT) Serial.print(F("[OcppOperation] (Conf) JSON message: "));
            if (DEBUG_OUT || TRAFFIC_OUT) Serial.println(out);
            onSendConfListener(confPayload->as<JsonObject>());
        } else {
            Serial.print(F("[OcppOperation] (Conf) Error occured! JSON CallError message: "));
            Serial.print(out);
            Serial.print('\n');
            onAbortListener();
        }
    }

    return wsSuccess;
}

void OcppOperation::setInitiated() {
    if (ocppMessage) {
        ocppMessage->initiate();
    } else {
        Serial.print(F("[OcppOperation] Error: called setInitiated without corresponding OcppOperation!\n"));
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

boolean OcppOperation::isFullyConfigured(){
    return ocppMessage != nullptr;
}

void OcppOperation::print_debug() {
    Serial.print(F("[OcppOperation] I am a "));
    if (ocppMessage) {
        Serial.print(ocppMessage->getOcppOperationType());
    } else {
        Serial.print(F("nullptr"));
    }
    Serial.print(F("\n"));
}
