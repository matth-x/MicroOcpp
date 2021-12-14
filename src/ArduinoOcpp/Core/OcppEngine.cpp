// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppModel.h>

#include <Variants.h>

using namespace ArduinoOcpp;

OcppEngine *ArduinoOcpp::defaultOcppEngine = nullptr;

OcppEngine::OcppEngine(OcppSocket& ocppSocket, const OcppClock& system_clock)
        : oSock(ocppSocket), oModel{std::make_shared<OcppModel>(system_clock)}, oConn{oSock, oModel} {
    defaultOcppEngine = this;
}

OcppEngine::~OcppEngine() {
    defaultOcppEngine = nullptr;
}

void OcppEngine::loop() {
    oSock.loop();
    oConn.loop(oSock);

    if (runOcppTasks)
        oModel->loop();
}

void OcppEngine::initiateOperation(std::unique_ptr<OcppOperation> op) {
    if (op) {
        op->setOcppModel(oModel);
        oConn.initiateOcppOperation(std::move(op));
    }
}

OcppModel& OcppEngine::getOcppModel() {
    return *oModel;
}
