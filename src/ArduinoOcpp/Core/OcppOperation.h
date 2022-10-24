// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OCPPOPERATION_H
#define OCPPOPERATION_H

#define MESSAGE_TYPE_CALL 2
#define MESSAGE_TYPE_CALLRESULT 3
#define MESSAGE_TYPE_CALLERROR 4

#include <memory>

#include <ArduinoOcpp/Core/OcppOperationCallbacks.h>

namespace ArduinoOcpp {

class OcppMessage;
class OcppModel;
class OcppSocket;
class StoredOperationHandler;

class OcppOperation {
private:
    std::string messageID {};
    std::unique_ptr<OcppMessage> ocppMessage;
    const std::string *getMessageID();
    void setMessageID(const std::string &id);
    OnReceiveConfListener onReceiveConfListener = [] (JsonObject payload) {};
    OnReceiveReqListener onReceiveReqListener = [] (JsonObject payload) {};
    OnSendConfListener onSendConfListener = [] (JsonObject payload) {};
    OnTimeoutListener onTimeoutListener = [] () {};
    OnReceiveErrorListener onReceiveErrorListener = [] (const char *code, const char *description, JsonObject details) {};
    OnAbortListener onAbortListener = [] () {};
    bool reqExecuted = false;

    std::unique_ptr<Timeout> timeout{new OfflineSensitiveTimeout(40000)};

    const ulong RETRY_INTERVAL = 3000; //in ms; first retry after ... ms; second retry after 2 * ... ms; third after 4 ...
    const ulong RETRY_INTERVAL_MAX = 20000; //in ms; 
    ulong retry_start = 0;
    ulong retry_interval_mult = 1; // RETRY_INTERVAL * retry_interval_mult gives longer periods with each iteration

    uint16_t printReqCounter = 0;

    std::unique_ptr<StoredOperationHandler> opStore;
public:

    OcppOperation(std::unique_ptr<OcppMessage> msg);

    OcppOperation();

    ~OcppOperation();

    void setOcppMessage(std::unique_ptr<OcppMessage> msg);

    void setOcppModel(std::shared_ptr<OcppModel> oModel);

    void setTimeout(std::unique_ptr<Timeout> timeout);

    Timeout *getTimeout();

    /**
     * Sends the message(s) that belong to the OCPP Operation. This function puts a JSON message on the lower protocol layer.
     * 
     * For instance operation Authorize: sends Authorize.req(idTag)
     * 
     * This function is usually called multiple times by the Arduino loop(). On first call, the request is initially sent. In the
     * succeeding calls, the implementers decide to either resend the request, or do nothing as the operation is still pending. When
     * the operation is completed (for example when conf() has been called), return true. When the operation is still pending, return
     * false.
     */
    bool sendReq(OcppSocket& ocppSocket);

   /**
    * Decides if message belongs to this operation instance and if yes, proccesses it. For example, multiple instances of an
    * operation type can run in the case of Metering Data being sent.
    * 
    * Returns true if JSON object has been consumed, false otherwise.
    */
    bool receiveConf(JsonDocument& json);

    /**
    * Decides if message belongs to this operation instance and if yes, notifies the OcppMessage object about the CallError.
    * 
    * Returns true if JSON object has been consumed, false otherwise.
    */
    bool receiveError(JsonDocument& json);

    /**
     * Processes the request in the JSON document. Returns true on success, false on error.
     * 
     * Returns false if the request doesn't belong to the corresponding operation instance
     */
    bool receiveReq(JsonDocument& json);

    /**
     * After processing a request sent by the communication counterpart, this function sends a confirmation
     * message. Returns true on success, false otherwise. Returns also true if a CallError has successfully
     * been sent
     */
    bool sendConf(OcppSocket& ocppSocket);

    void initiate(std::unique_ptr<StoredOperationHandler> opStorage);

    bool restore(std::unique_ptr<StoredOperationHandler> opStorage, std::shared_ptr<OcppModel> oModel);

    StoredOperationHandler *getStorageHandler() {return opStore.get();}

    void finalize(); //called whenever this operation is completed or aborted right before the destructor

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

    bool isFullyConfigured();

    void rebaseMsgId(int msgIdCounter); //workaround; remove when random UUID msg IDs are introduced

    void print_debug();
};

} //end namespace ArduinoOcpp

 #endif
