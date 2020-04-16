#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include "OcppOperation.h"

class Authorize : public OcppOperation {
private:
  boolean waitForConf = false;
  boolean completed = false;
  boolean reqExecuted = false; // For debugging only: implement dummy server functionalities to test against echo server
public:
  Authorize(WebSocketsClient *webSocket);

  /**
   * See OcppOperation.h for more information
   */
  boolean sendReq();
  boolean receiveConf(JsonDocument *json);

  /**
   * For debugging only: implement dummy server functionalities to test against echo server
   */
  boolean receiveReq(JsonDocument *json);
  boolean sendConf();
};



#endif
