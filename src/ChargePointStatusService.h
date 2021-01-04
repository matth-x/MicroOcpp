// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef CHARGEPOINTSTATUSSERVICE_H
#define CHARGEPOINTSTATUSSERVICE_H

#include <WebSocketsClient.h>
#include <LinkedList.h>

#include "ConnectorStatus.h"

class ChargePointStatusService {
private:
  WebSocketsClient *webSocket;
  const int numConnectors;
  ConnectorStatus **connectors;

  boolean authorized = false;
  String idTag = String('\0');

public:
  ChargePointStatusService(WebSocketsClient *webSocket, int numConnectors);

  ~ChargePointStatusService();
  
  void loop();

  void authorize(String &idTag);
  void authorize();
  void boot();
  String &getUnboundIdTag();
  boolean existsUnboundAuthorization();
  void bindAuthorization(String &idTag, int connectorId);

  ConnectorStatus *getConnector(int connectorId);
  int getNumConnectors();

  void recoverState();
};

#endif
