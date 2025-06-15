// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUEST_H
#define MO_REQUEST_H

#define MESSAGE_TYPE_CALL 2
#define MESSAGE_TYPE_CALLRESULT 3
#define MESSAGE_TYPE_CALLERROR 4

#include <memory>

#include <MicroOcpp/Core/RequestCallbacks.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;
class Operation;

class Request : public MemoryManaged {
private:
    Context& context;
    char *messageID = nullptr;
    std::unique_ptr<Operation> operation;
    bool setMessageID(const char *id);
    ReceiveConfListener onReceiveConfListener;
    void (*onReceiveReqListener)(const char *operationType, const char *payloadJson, void *userData) = nullptr;
    void *onReceiveReqListenerUserData = nullptr;
    void (*onSendConfListener)(const char *operationType, const char *payloadJson, void *userData) = nullptr;
    void *onSendConfListenerUserData = nullptr;
    TimeoutListener onTimeoutListener;
    ReceiveErrorListener onReceiveErrorListener;
    AbortListener onAbortListener;

    Timestamp timeoutStart;
    int32_t timeoutPeriod = 40; //in seconds
    bool timedOut = false;
    
    Timestamp debugRequestTime;

    bool requestSent = false;
public:

    Request(Context& context, std::unique_ptr<Operation> msg);

    ~Request();

    Operation *getOperation();

    void setTimeout(int32_t timeout); //if positive, sets timeout period in secs. If 0 or negative value, disables timeout
    bool isTimeoutExceeded();
    void executeTimeout(); //call Timeout Listener
    void setOnTimeout(TimeoutListener onTimeout);

    /**
     * Sends the message(s) that belong to the OCPP Operation. This function puts a JSON message on the lower protocol layer.
     * 
     * For instance operation Authorize: sends Authorize.req(idTag)
     * 
     * This function is usually called multiple times by the Arduino loop(). On first call, the request is initially sent. In the
     * succeeding calls, the implementers decide to either resend the request, or do nothing as the operation is still pending.
     */
    enum class CreateRequestResult {
        Success,
        Failure
    };
    CreateRequestResult createRequest(JsonDoc& out);

   /**
    * Processes Server response (=Confirmations and Errors). Returns true on success, false otherwise
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
    enum class CreateResponseResult {
        Success,
        Pending,
        Failure
    };

    CreateResponseResult createResponse(JsonDoc& out);

    void setOnReceiveConf(ReceiveConfListener onReceiveConf); //listener executed when we received the .conf() to a .req() we sent
    void setOnReceiveRequest(void (*onReceiveReq)(const char *operationType, const char *payloadJson, void *userData), void *userData); //listener executed when we receive a .req()
    void setOnSendConf(void (*onSendConf)(const char *operationType, const char *payloadJson, void *userData), void *userData); //listener executed when we send a .conf() to a .req() we received

    void setReceiveErrorListener(ReceiveErrorListener onReceiveError);

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
    void setOnAbort(AbortListener onAbort);

    const char *getOperationType();

    void setRequestSent();
    bool isRequestSent();
};

/*
 * Simple factory functions
 */
std::unique_ptr<Request> makeRequest(Context& context, std::unique_ptr<Operation> op);
std::unique_ptr<Request> makeRequest(Context& context, Operation *op); //takes ownership of op

} //namespace MicroOcpp

 #endif
