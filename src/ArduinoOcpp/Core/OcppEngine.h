// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPENGINE_H
#define OCPPENGINE_H

#include <ArduinoOcpp/Core/OcppConnection.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

class OcppSocket;
class OcppModel;

class OcppEngine {
private:
    OcppSocket& oSock;
    std::shared_ptr<OcppModel> oModel;
    OcppConnection oConn;

    bool runOcppTasks = true;
public:
    OcppEngine(OcppSocket& ocppSocket, const OcppClock& system_clock);
    ~OcppEngine();

    void loop();

    void setRunOcppTasks(bool enable) {runOcppTasks = enable;}

    void initiateOperation(std::unique_ptr<OcppOperation> op);

    OcppModel& getOcppModel();
};

extern OcppEngine *defaultOcppEngine;

} //end namespace ArduinoOcpp

#endif
