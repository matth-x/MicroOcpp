// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef DATATRANSFER_SERVICE_H
#define DATATRANSFER_SERVICE_H

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>
#include <MicroOcpp/Context.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {
struct DataTransferOperationCreator {
    const char *venderId = nullptr;
    const char *messageId = nullptr;
    Operation*(*create)(Context& context) = nullptr;
};

class DataTransferService : public MemoryManaged {
private:
    Context& context;
    Vector<DataTransferOperationCreator> operationRegistry;

public:
    DataTransferService(Context& context);

    bool setup();

    void loop();

    bool registerOperation(const char *venderId, const char *messageId, Operation*(*createOperationCb)(Context& context));

    Operation* getRegisteredOperation(const char *venderId, const char *messageId);

    void clearRegisteredOperation(const char *venderId, const char *messageId);
};
} //namespace MicroOcpp

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif //DATATRANSFER_SERVICE_H
