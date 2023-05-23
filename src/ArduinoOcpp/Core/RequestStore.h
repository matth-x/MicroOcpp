// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OPERATIONSTORE_H
#define OPERATIONSTORE_H

#include <memory>
#include <deque>
#include <ArduinoJson.h>

#define AO_MAX_OPNR 10000

namespace ArduinoOcpp {

class RequestStore;
class FilesystemAdapter;
template<class T> class Configuration;

class StoredOperationHandler {
private:
    RequestStore& context;
    int opNr = -1;
    std::shared_ptr<FilesystemAdapter> filesystem;

    std::unique_ptr<DynamicJsonDocument> rpc;
    std::unique_ptr<DynamicJsonDocument> payload;

    bool isPersistent = false;

public:
    StoredOperationHandler(RequestStore& context, std::shared_ptr<FilesystemAdapter> filesystem) : context(context), filesystem(filesystem) {}

    void setRpcData(std::unique_ptr<DynamicJsonDocument> rpc) {this->rpc = std::move(rpc);}
    void setPayload(std::unique_ptr<DynamicJsonDocument> payload) {this->payload = std::move(payload);}

    std::unique_ptr<DynamicJsonDocument> getRpcData() {return std::move(rpc);}
    std::unique_ptr<DynamicJsonDocument> getPayload() {return std::move(payload);}

    bool commit();
    void clearBuffer() {rpc.reset(); payload.reset();}

    bool restore(unsigned int opNr);

    int getOpNr() {return isPersistent ? opNr : -1;}
};

class RequestStore {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;
    std::shared_ptr<Configuration<int>> opBegin; //Tx-related operations are stored; index of the first pending operation
    unsigned int opEnd = 0; //one place after last number

public:
    RequestStore() = delete;
    RequestStore(std::shared_ptr<FilesystemAdapter> filesystem);

    std::unique_ptr<StoredOperationHandler> makeOpHandler();
    std::unique_ptr<StoredOperationHandler> fetchOpHandler(unsigned int opNr);

    unsigned int reserveOpNr();
    void advanceOpNr(unsigned int oldOpNr);

    unsigned int getOpBegin();
    unsigned int getOpEnd() {return opEnd;}
};

}

#endif
