// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef OCPPOPERATION_H
#define OCPPOPERATION_H

#define MESSAGE_TYPE_CALL 2
#define MESSAGE_TYPE_CALLRESULT 3
#define MESSAGE_TYPE_CALLERROR 4

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "OcppMessage.h"

typedef void (*OnReceiveConfListener)(JsonObject payload);
typedef void (*OnReceiveReqListener)(JsonObject payload);
typedef void (*OnSendConfListener)(JsonObject payload);


class OcppOperation {
private:
  String messageID;
  OcppMessage *ocppMessage = NULL;
  WebSocketsClient *webSocket;
  String &getMessageID();
  void setMessageID(String &id);
  OnReceiveConfListener onReceiveConfListener = NULL;
  OnReceiveReqListener onReceiveReqListener = NULL;
  OnSendConfListener onSendConfListener = NULL;
  boolean reqExecuted = false;

  const ulong TIMEOUT_CANCEL = 20000; //in ms; Cancel operation after ... ms
  boolean timeout_active = false;
  ulong timeout_start;

  const ulong RETRY_INTERVAL = 3000; //in ms; first retry after ... ms; second retry after 2 * ... ms; third after 4 ...
  ulong retry_start = 0;
  ulong retry_interval_mult = 1; // RETRY_INTERVAL * retry_interval_mult gives longer periods with each iteration
public:

  OcppOperation(WebSocketsClient *webSocket, OcppMessage *ocppMessage);

  OcppOperation(WebSocketsClient *webSocket);

  ~OcppOperation();

  void setOcppMessage(OcppMessage *msg);


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
     * Processes the request in the JSON document. Returns true on success, false on error.
     * 
     * Returns false if the request doesn't belong to the corresponding operation instance
     */
    boolean receiveReq(JsonDocument *json);

    /**
     * After successfully processing a request sent by the communication counterpart, this function sends a confirmation
     * message. Returns true on success, false otherwise.
     */
    boolean sendConf();

    void setOnReceiveConfListener(OnReceiveConfListener onReceiveConf);

    /**
     * Sets a Listener that is called after this machine processed a request by the communication counterpart
     */
    void setOnReceiveReqListener(OnReceiveReqListener onReceiveReq);

    void setOnSendConfListener(OnSendConfListener onSendConf);

    boolean isFullyConfigured();

    void print_debug();
};

 #endif
