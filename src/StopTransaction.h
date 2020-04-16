#ifndef STOPTRANSACTION_H
#define STOPTRANSACTION_H

#include "OcppOperation.h"

class StopTransaction : public OcppOperation {
private:
  boolean waitForConf = false;
  boolean completed = false;
  boolean reqExecuted = false; // For debugging only: implement dummy server functionalities to test against echo server
public:
  StopTransaction(WebSocketsClient *webSocket);

  /**
   * See OcppOperation.h for more information
   */
  boolean sendReq();
  boolean receiveConf(JsonDocument *json);

  /**
   * For debuggin only: implement dummy server functionalities to test against echo server
   */
  boolean receiveReq(JsonDocument *json);
  boolean sendConf();
};

#endif
