#ifndef STATUSNOTIFICATION_H
#define STATUSNOTIFICATION_H

#include "OcppOperation.h"
#include "ChargePointStatusService.h"
#include "TimeHelper.h"

class StatusNotification : public OcppOperation {
private:
  boolean waitForConf = false;
  boolean completed = false;
  boolean reqExecuted = false; // For debugging only: implement dummy server functionalities to test against echo server
  ChargePointStatus currentStatus = ChargePointStatus::NOT_SET;
  char timestamp[JSONDATE_LENGTH + 1] = {'\0'};
public:
  StatusNotification(WebSocketsClient *webSocket, ChargePointStatus currentStatus);

  /**
   * See OcppOperation.h for more information
   */
  boolean sendReq();
  boolean receiveConf(JsonDocument *json);

  /**
   * For debuggin only: implement dummy server functionalities to test against echo server
   */
  StatusNotification(WebSocketsClient *webSocket);
  boolean receiveReq(JsonDocument *json);
  boolean sendConf();
};

#endif
