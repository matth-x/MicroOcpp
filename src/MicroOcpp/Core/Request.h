// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_REQUEST_H
#define MO_REQUEST_H

#define MESSAGE_TYPE_CALL 2
#define MESSAGE_TYPE_CALLRESULT 3
#define MESSAGE_TYPE_CALLERROR 4

#include <memory>

#include <MicroOcpp/Core/RequestCallbacks.h>

namespace MicroOcpp {

class Operation;
class Model;
class StoredOperationHandler;

class Request {
private:
    std::string messageID {};
    std::unique_ptr<Operation> operation;
    void setMessageID(const std::string &id);
    OnReceiveConfListener onReceiveConfListener = [] (JsonObject payload) {};
    OnReceiveReqListener onReceiveReqListener = [] (JsonObject payload) {};
    OnSendConfListener onSendConfListener = [] (JsonObject payload) {};
    OnTimeoutListener onTimeoutListener = [] () {};
    OnReceiveErrorListener onReceiveErrorListener = [] (const char *code, const char *description, JsonObject details) {};
    OnAbortListener onAbortListener = [] () {};

    unsigned long timeout_start = 0;
    unsigned long timeout_period = 40000;
    bool timed_out = false;

    unsigned int trialNo = 0;
    
    unsigned long debugRequest_start = 0;

    std::unique_ptr<StoredOperationHandler> opStore;
public:

    Request(std::unique_ptr<Operation> msg);

    Request();

    ~Request();

    void setOperation(std::unique_ptr<Operation> msg);

    void setTimeout(unsigned long timeout); //0 = disable timeout
    bool isTimeoutExceeded();
    void executeTimeout(); //call Timeout handler

    unsigned int getTrialNo(); //how many times createRequest() has been tried (used for retry behavior)

    /**
     * Sends the message(s) that belong to the OCPP Operation. This function puts a JSON message on the lower protocol layer.
     * 
     * For instance operation Authorize: sends Authorize.req(idTag)
     * 
     * This function is usually called multiple times by the Arduino loop(). On first call, the request is initially sent. In the
     * succeeding calls, the implementers decide to either resend the request, or do nothing as the operation is still pending.
     */
    std::unique_ptr<DynamicJsonDocument> createRequest();

   /**
    * Decides if message belongs to this operation instance and if yes, proccesses it. Receives both Confirmations and Errors
    * 
    * Returns true if JSON object has been consumed, false otherwise.
    */
    bool receiveResponse(JsonArray json);

    /**
     * Processes the request in the JSON document. Returns true on success, false on error.
     * 
     * Returns false if the request doesn't belong to the corresponding operation instance
     */
    bool receiveRequest(JsonArray json);

    /**
     * After processing a request sent by the communication counterpart, this function sends a confirmation
     * message. Returns true on success, false otherwise. Returns also true if a CallError has successfully
     * been sent
     */
    std::unique_ptr<DynamicJsonDocument> createResponse();

    void initiate(std::unique_ptr<StoredOperationHandler> opStorage);

    bool restore(std::unique_ptr<StoredOperationHandler> opStorage, Model *model);

    StoredOperationHandler *getStorageHandler() {return opStore.get();}

    void setOnReceiveConfListener(OnReceiveConfListener onReceiveConf);

    /**
     * Sets a Listener that is called after this machine processed a request by the communication counterpart
     */
    void setOnReceiveReqListener(OnReceiveReqListener onReceiveReq);

    void setOnSendConfListener(OnSendConfListener onSendConf);

    void setOnTimeoutListener(OnTimeoutListener onTimeout);

    void setOnReceiveErrorListener(OnReceiveErrorListener onReceiveError);

    /**
     * The listener onAbort will be called whenever the engine stops trying to execute an operation normally which were initiated
     * on this device. This includes timeouts or if the ocpp counterpart sends an error (then it will be called in addition to
     * onTimeout or onReceiveError, respectively). Causes for onAbort:
     * 
     *    - Cannot create OCPP payload
     *    - Timeout
     *    - Receives error msg instead of confirmation msg
     * 
     * The engine uses this listener in both modes: EVSE mode and Central system mode
     */
    void setOnAbortListener(OnAbortListener onAbort);

    const char *getMessageID();
    const char *getOperationType();
};

} //end namespace MicroOcpp

 #endif
