// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppModel.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

OcppEngine *ArduinoOcpp::defaultOcppEngine = nullptr;

OcppEngine::OcppEngine(OcppSocket& ocppSocket, const OcppClock& system_clock, std::shared_ptr<FilesystemAdapter> filesystem)
        : oSock(ocppSocket), oModel{std::make_shared<OcppModel>(system_clock)}, oConn{operationDeserializer, oModel, filesystem} {
    defaultOcppEngine = this;

    preBootConn = std::unique_ptr<OcppConnection>(new OcppConnection(operationDeserializer, oModel, nullptr)); //pre boot queue doesn't need persistency
    preBootConn->setSocket(oSock);
}

OcppEngine::~OcppEngine() {
    defaultOcppEngine = nullptr;
}

void OcppEngine::loop() {
    oSock.loop();

    if (preBootConn) {
        preBootConn->loop(oSock);
    } else {
        oConn.loop(oSock);
    }

    oModel->loop();
}

void OcppEngine::activatePostBootCommunication() {
    //switch from pre boot connection to normal connetion
    oConn.setSocket(oSock);
    preBootConn.reset();
}

void OcppEngine::initiateOperation(std::unique_ptr<OcppOperation> op) {
    if (op) {
        oConn.sendRequest(std::move(op));
    }
}

void OcppEngine::initiatePreBootOperation(std::unique_ptr<OcppOperation> op) {
    if (op) {
        if (preBootConn) {
            preBootConn->sendRequest(std::move(op));
        } else {
            //not in pre boot mode anymore - initiate normally
            initiateOperation(std::move(op));
        }
    }
}

OcppModel& OcppEngine::getOcppModel() {
    return *oModel;
}

OperationDeserializer& OcppEngine::getOperationDeserializer() {
    return operationDeserializer;
}
