// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OperationStore.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#ifndef AO_OPSTORE_DIR
#define AO_OPSTORE_DIR AO_FILENAME_PREFIX "/"
#endif

#define AO_OPSTORE_FN AO_FILENAME_PREFIX "/opstore.cnf"

#define AO_MAX_OPNR 10000

using namespace ArduinoOcpp;

bool StoredOperationHandler::commit() {
    if (isPersistent) {
        AO_DBG_ERR("cannot call two times");
        return false;
    }
    if (!filesystem) {
        AO_DBG_DEBUG("filesystem");
        return false;
    }

    if (!rpc || !payload) {
        AO_DBG_ERR("unitialized");
        return false;
    }

    opNr = context.reserveOpNr();

    char fn [MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MAX_PATH_SIZE, AO_OPSTORE_DIR "op" "-%u.jsn", opNr);
    if (ret < 0 || ret >= MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    DynamicJsonDocument doc {JSON_OBJECT_SIZE(2) + rpc->capacity() + payload->capacity()};
    doc["rpc"] = *rpc;
    doc["payload"] = *payload;

    if (!FilesystemUtils::storeJson(filesystem, fn, doc)) {
        AO_DBG_ERR("FS error");
        return false;
    }

    isPersistent = true;
    return true;
}

bool StoredOperationHandler::restore(unsigned int opNrToLoad) {
    if (isPersistent) {
        AO_DBG_ERR("cannot restore after commit");
        return false;
    }
    if (!filesystem) {
        AO_DBG_DEBUG("filesystem");
        return false;
    }

    opNr = opNrToLoad;

    char fn [MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MAX_PATH_SIZE, AO_OPSTORE_DIR "op" "-%u.jsn", opNr);
    if (ret < 0 || ret >= MAX_PATH_SIZE) {
        AO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        AO_DBG_DEBUG("operation %u does not exist", opNr);
        return false;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn);
    if (!doc) {
        AO_DBG_ERR("FS error");
        return false;
    }
    
    JsonVariant rpc_restore = (*doc)["rpc"];
    JsonVariant payload_restore = (*doc)["payload"];

    rpc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(rpc_restore.memoryUsage()));
    payload = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(payload_restore.memoryUsage()));
    
    *rpc = rpc_restore;
    *payload = payload_restore;

    isPersistent = true;
    return true;
}

OperationStore::OperationStore(std::shared_ptr<FilesystemAdapter> filesystem) : filesystem(filesystem) {
    opBegin = declareConfiguration<int>("AO_opBegin", 0, AO_OPSTORE_FN, false, false, true, false);

    if (!opBegin || *opBegin < 0) {
        AO_DBG_ERR("init failure");
    } else if (filesystem) {
        opEnd = *opBegin;

        unsigned int misses = 0;
        unsigned int i = opEnd;
        while (misses < 3) {
            char fn [MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, MAX_PATH_SIZE, AO_OPSTORE_DIR "op" "-%u.jsn", i);
            if (ret < 0 || ret >= MAX_PATH_SIZE) {
                AO_DBG_ERR("fn error: %i", ret);
                misses++;
                i++;
                continue;
            }

            size_t msize;
            if (filesystem->stat(fn, &msize) != 0) {
                AO_DBG_DEBUG("operation %u does not exist", i);
                misses++;
                i++;
                continue;
            }

            //file exists
            misses = 0;
            opEnd = i;
            i++;
        }
    }
}

std::unique_ptr<StoredOperationHandler> OperationStore::makeOpHandler() {
    return std::unique_ptr<StoredOperationHandler>(new StoredOperationHandler(*this, filesystem));
}

unsigned int OperationStore::reserveOpNr() {
    AO_DBG_DEBUG("reserved opNr %u", opEnd);
    auto res = opEnd;
    opEnd++;
    opEnd %= AO_MAX_OPNR;
    return res;
}

void OperationStore::advanceOpNr(unsigned int oldOpNr) {
    if (!opBegin || *opBegin < 0) {
        AO_DBG_ERR("init failure");
        return;
    }

    if (oldOpNr != (unsigned int) *opBegin) {
        if (oldOpNr - (unsigned int) *opBegin < 100) {
            AO_DBG_ERR("synchronization failure - try to fix");
            (void)0;
        } else {
            AO_DBG_ERR("synchronization failure");
            return;
        }
    }

    unsigned int opNr = (oldOpNr + 1) % AO_MAX_OPNR;

    AO_DBG_DEBUG("advance opBegin: %u", opNr);
    *opBegin = opNr;
    configuration_save();
}

unsigned int OperationStore::getOpBegin() {
    if (!opBegin || *opBegin < 0) {
        AO_DBG_ERR("invalid state");
        return 0;
    }
    return (unsigned int) *opBegin;
}
