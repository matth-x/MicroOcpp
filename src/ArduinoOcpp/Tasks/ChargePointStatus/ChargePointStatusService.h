// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHARGEPOINTSTATUSSERVICE_H
#define CHARGEPOINTSTATUSSERVICE_H

#include <ArduinoOcpp/Tasks/ChargePointStatus/ConnectorStatus.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Platform.h>

#include <vector>

namespace ArduinoOcpp {

class OcppEngine;

class ChargePointStatusService {
private:
    OcppEngine& context;
    
    std::vector<std::unique_ptr<ConnectorStatus>> connectors;

    std::function<bool(bool isHard)> preReset; //true: reset is possible; false: reject reset; Await: need more time to determine
    std::function<void(bool isHard)> executeReset; //please disconnect WebSocket (AO remains initialized), shut down device and restart with normal initialization routine; on failure reconnect WebSocket
    unsigned int outstandingResetRetries = 0; //0 = do not reset device
    bool isHardReset = false;
    unsigned long t_resetRetry;

    std::shared_ptr<Configuration<int>> resetRetries;

public:
    ChargePointStatusService(OcppEngine& context, unsigned int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    ~ChargePointStatusService();
    
    void loop();

    ConnectorStatus *getConnector(int connectorId);
    int getNumConnectors();

    void setPreReset(std::function<bool(bool isHard)> preReset);
    std::function<bool(bool isHard)> getPreReset();

    void setExecuteReset(std::function<void(bool isHard)> executeReset);
    std::function<void(bool isHard)> getExecuteReset();

    void initiateReset(bool isHard);
};

} //end namespace ArduinoOcpp

#if AO_PLATFORM == AO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))

namespace ArduinoOcpp {

std::function<void(bool isHard)> makeDefaultResetFn();

}

#endif //AO_PLATFORM == AO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))

#endif
