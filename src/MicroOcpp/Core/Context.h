// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CONTEXT_H
#define MO_CONTEXT_H

#include <memory>

#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Version.h>

namespace MicroOcpp {

class Connection;
class FilesystemAdapter;

class Context : public MemoryManaged {
private:
    Connection& connection;
    OperationRegistry operationRegistry;
    Model model;
    RequestQueue reqQueue;

    std::unique_ptr<FtpClient> ftpClient;

public:
    Context(Connection& connection, std::shared_ptr<FilesystemAdapter> filesystem, uint16_t bootNr, ProtocolVersion version);
    ~Context();

    void loop();

    void initiateRequest(std::unique_ptr<Request> op);

    Model& getModel();

    OperationRegistry& getOperationRegistry();

    const ProtocolVersion& getVersion();

    Connection& getConnection();

    RequestQueue& getRequestQueue();

    void setFtpClient(std::unique_ptr<FtpClient> ftpClient);
    FtpClient *getFtpClient();
};

} //end namespace MicroOcpp

#endif
