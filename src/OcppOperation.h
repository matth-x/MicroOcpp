/**
 * This is the superclass to all OCPP Operations in this framework. The client code (e.g. the CP) creates an instance of an
 * OcppOperation each time it invokes the framework to send an OCPP message. The framework calls req() and conf() when it is
 * ready to send the message, or when it received any message, respectively.
 * 
 */

 #ifndef OCPPOPERATION_H
 #define OCPPOPERATION_H

#define MESSAGE_TYPE_CALL 2
#define MESSAGE_TYPE_CALLRESULT 3
#define MESSAGE_TYPE_CALLERROR 4

 #include<ArduinoJson.h>
 #include<WebSocketsClient.h>

typedef void (*OnCompleteListener)(JsonDocument *json);
typedef void (*OnReceiveReqListener)(JsonDocument *json);


class OcppOperation {
private:
  int messageID = -1;
protected:
  WebSocketsClient *webSocket;
  int getMessageID();
  void setMessageID(int id);
  OnCompleteListener onCompleteListener = NULL;
  OnReceiveReqListener onReceiveReqListener = NULL;
public:

  OcppOperation(WebSocketsClient *webSocket);

  virtual ~OcppOperation();


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
   virtual boolean sendReq();

   /**
    * Decides if message belongs to this operation instance and if yes, proccesses it. For example, multiple instances of an
    * operation type can run in the case of Metering Data being sent.
    * 
    * Returns true if JSON object has been consumed, false otherwise.
    */
    virtual boolean receiveConf(JsonDocument *json);

    /**
     * Processes the request in the JSON document. Returns true on success, false on error.
     * 
     * Returns false if the request doesn't belong to the corresponding operation instance
     */
    virtual boolean receiveReq(JsonDocument *json);

    /**
     * After successfully processing a request sent by the communication counterpart, this function sends a confirmation
     * message. Returns true on success, false otherwise.
     */
    virtual boolean sendConf();

    /**
     * Sets a Listener that is called when the operation is finished. The listener is passed the JSON from
     * a) the OCPP communication counterpart in case the counterpart passed it as answer
     * b) this OCPP station in case this station has sent it as request (i.e. it is the copy of the JSON document
     *    generated on this device before)
     */
    void setOnCompleteListener(OnCompleteListener onComplete);

    /**
     * Sets a Listener that is called after this machine processed a request by the communication counterpart
     */
    void setOnReceiveReq(OnReceiveReqListener onReceiveReq);
};

 #endif
