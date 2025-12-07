#include <MicroOcpp/Model/DataTransfer/DataTransferService.h>
#include <MicroOcpp/Operations/DataTransfer.h>
#include <MicroOcpp/Debug.h>

#include <cstring>

using namespace MicroOcpp;

DataTransferService::DataTransferService(Context& context) 
    :  MemoryManaged("v16/v201.DataTransfer.DataTransferService"),
       context(context)
{
}

bool DataTransferService::setup()
{
    // Register DataTransfer operation
    context.getMessageService().registerOperation("DataTransfer", [] (Context& context) -> Operation* {
        return new DataTransfer(context);});

    operationRegistry.clear();

    return true;
}

void DataTransferService::loop()
{
    // do nothing
}

bool DataTransferService::registerOperation(const char *venderId, const char *messageId, Operation*(*createOperationCb)(Context& context))
{
    bool exists = false;

    for (auto& op : operationRegistry) {
        if (std::strcmp(venderId, op.venderId) == 0 && std::strcmp(messageId, op.messageId) == 0) {
            exists = true;
            break;
        }
    }

    if (exists) {
        MO_DBG_DEBUG("DataTransfer operation creator venderId %s, messageId %s, already exists - do not replace", venderId, messageId);
        return true;
    }

    DataTransferOperationCreator entry;
    entry.venderId = venderId;
    entry.messageId = messageId;
    entry.create = createOperationCb;

    MO_DBG_DEBUG("registered DataTransfer operation venderId %s, messageId %s", venderId, messageId);
    bool capacity = operationRegistry.size() + 1;
    operationRegistry.reserve(capacity);
    if (operationRegistry.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }

    operationRegistry.push_back(entry);
    return true;
}

Operation* DataTransferService::getRegisteredOperation(const char *venderId, const char *messageId)
{
    for (auto& op : operationRegistry) {
        if (std::strcmp(venderId, op.venderId) == 0 && std::strcmp(messageId, op.messageId) == 0) {
            return op.create(context);
        }
    }

    return nullptr;
}


void DataTransferService::clearRegisteredOperation(const char *venderId, const char *messageId)
{
    for (auto it = operationRegistry.begin(); it != operationRegistry.end();) {
        if (std::strcmp(venderId, it->venderId) == 0 && std::strcmp(messageId, it->messageId) == 0) {
            it = operationRegistry.erase(it);
        } else {
            it++;
        }
    }
}