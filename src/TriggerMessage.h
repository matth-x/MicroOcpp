#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include "Variants.h"

#include "OcppOperation.h"

class TriggerMessage : public OcppOperation {
private:
  boolean waitForConf = false;
  boolean completed = false;
  boolean reqExecuted = false;
  OcppOperation *triggeredOperation;
  char *statusMessage;
public:
  TriggerMessage(WebSocketsClient *webSocket);

  /**
   * See OcppOperation.h for more information
   */
  boolean receiveReq(JsonDocument *json);
  boolean sendConf();
};



#endif
