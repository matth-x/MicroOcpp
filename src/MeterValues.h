#ifndef METERVALUES_H
#define METERVALUES_H

#include "OcppOperation.h"
#include <LinkedList.h>
#include "TimeHelper.h"

class MeterValues : public OcppOperation {
private:
  boolean waitForConf = false;
  boolean completed = false;
  boolean reqExecuted = false; // For debugging only: implement dummy server functionalities to test against echo server

  LinkedList<time_t> powerMeasurementTime;
  LinkedList<float> power;

public:
  MeterValues(WebSocketsClient *webSocket, LinkedList<time_t> *powerMeasurementTime, LinkedList<float> *power);

  MeterValues(WebSocketsClient *webSocket); //for debugging only. Make this for the server pendant

  ~MeterValues();


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
