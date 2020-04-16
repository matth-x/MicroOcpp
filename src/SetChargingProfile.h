#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include "OcppOperation.h"
#include "SmartChargingService.h"

class SetChargingProfile : public OcppOperation {
private:
  boolean waitForConf = false;
  boolean completed = false;
  boolean reqExecuted = false;
  SmartChargingService *smartChargingService;
public:
  SetChargingProfile(WebSocketsClient *webSocket, SmartChargingService *smartChargingService);

  /**
   * See OcppOperation.h for more information
   */
  boolean receiveReq(JsonDocument *json);
  boolean sendConf();
};



#endif
