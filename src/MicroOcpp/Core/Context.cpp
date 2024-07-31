// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Model.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Context::Context(Connection& connection, std::shared_ptr<FilesystemAdapter> filesystem, uint16_t bootNr, ProtocolVersion version)
        : connection(connection), model{version, bootNr}, reqQueue{connection, operationRegistry} {

}

Context::~Context() {

}

void Context::loop() {
    connection.loop();
    reqQueue.loop();
    model.loop();
}

void Context::initiateRequest(std::unique_ptr<Request> op) {
    if (!op) {
        MO_DBG_ERR("invalid arg");
        return;
    }
    reqQueue.sendRequest(std::move(op));
}

Model& Context::getModel() {
    return model;
}

OperationRegistry& Context::getOperationRegistry() {
    return operationRegistry;
}

const ProtocolVersion& Context::getVersion() {
    return model.getVersion();
}

Connection& Context::getConnection() {
    return connection;
}

RequestQueue& Context::getRequestQueue() {
    return reqQueue;
}

void Context::setFtpClient(std::unique_ptr<FtpClient> ftpClient) {
    this->ftpClient = std::move(ftpClient);
}

FtpClient *Context::getFtpClient() {
    return ftpClient.get();
}
