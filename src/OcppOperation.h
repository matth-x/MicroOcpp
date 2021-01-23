// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPOPERATION_H
#define OCPPOPERATION_H

#define MESSAGE_TYPE_CALL 2
#define MESSAGE_TYPE_CALLRESULT 3
#define MESSAGE_TYPE_CALLERROR 4

#include <functional>

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "OcppMessage.h"
#include "OcppOperationTimeout.h"

#include "Variants.h"

typedef std::function<void(JsonObject payload)> OnReceiveConfListener;
typedef std::function<void(JsonObject payload)> OnReceiveReqListener;
typedef std::function<void(JsonObject payload)> OnSendConfListener;
typedef std::function<void()> OnTimeoutListener;
typedef std::function<void(const char *code, const char *description, JsonObject details)> OnReceiveErrorListener; //will be called if OCPP communication partner returns error code
typedef std::function<void()> OnAbortListener; //will be called whenever the engine will stop trying to execute the operation normallythere is a timeout or error (onAbort = onTimeout || onReceiveError)


class OcppOperation {
private:
  String messageID;
  OcppMessage *ocppMessage = NULL;
  WebSocketsClient *webSocket;
  String &getMessageID();
  void setMessageID(String &id);
  OnReceiveConfListener onReceiveConfListener = [] (JsonObject payload) {};
  OnReceiveReqListener onReceiveReqListener = [] (JsonObject payload) {};
  OnSendConfListener onSendConfListener = [] (JsonObject payload) {};
  OnTimeoutListener onTimeoutListener = [] () {};
  OnReceiveErrorListener onReceiveErrorListener = [] (const char *code, const char *description, JsonObject details) {};
  OnAbortListener onAbortListener = [] () {};
  boolean reqExecuted = false;

  Timeout *timeout = new OfflineSensitiveTimeout(40000);

  const ulong RETRY_INTERVAL = 3000; //in ms; first retry after ... ms; second retry after 2 * ... ms; third after 4 ...
  const ulong RETRY_INTERVAL_MAX = 20000; //in ms; 
  ulong retry_start = 0;
  ulong retry_interval_mult = 1; // RETRY_INTERVAL * retry_interval_mult gives longer periods with each iteration
#if DEBUG_OUT
  uint16_t printReqCounter = -1;
#endif
public:

  OcppOperation(WebSocketsClient *webSocket, OcppMessage *ocppMessage);

  OcppOperation(WebSocketsClient *webSocket);

  ~OcppOperation();

  void setOcppMessage(OcppMessage *msg);

  void setTimeout(Timeout *timeout);

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
   boolean sendReq();

   /**
    * Decides if message belongs to this operation instance and if yes, proccesses it. For example, multiple instances of an
    * operation type can run in the case of Metering Data being sent.
    * 
    * Returns true if JSON object has been consumed, false otherwise.
    */
    boolean receiveConf(JsonDocument *json);

    /**
    * Decides if message belongs to this operation instance and if yes, notifies the OcppMessage object about the CallError.
    * 
    * Returns true if JSON object has been consumed, false otherwise.
    */
    boolean receiveError(JsonDocument *json);

    /**
     * Processes the request in the JSON document. Returns true on success, false on error.
     * 
     * Returns false if the request doesn't belong to the corresponding operation instance
     */
    boolean receiveReq(JsonDocument *json);

    /**
     * After processing a request sent by the communication counterpart, this function sends a confirmation
     * message. Returns true on success, false otherwise. Returns also true if a CallError has successfully
     * been sent
     */
    boolean sendConf();

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

    boolean isFullyConfigured();

    void print_debug();
};

 #endif
