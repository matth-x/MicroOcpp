// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/RequestStore.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

#define MOCPP_OPSTORE_FN MOCPP_FILENAME_PREFIX "opstore.cnf"

#define MOCPP_OPHISTORY_SIZE 3

using namespace MicroOcpp;

bool StoredOperationHandler::commit() {
    if (isPersistent) {
        MOCPP_DBG_ERR("cannot call two times");
        return false;
    }
    if (!filesystem) {
        MOCPP_DBG_DEBUG("filesystem");
        return false;
    }

    if (!rpc || !payload) {
        MOCPP_DBG_ERR("unitialized");
        return false;
    }

    opNr = context.reserveOpNr();

    char fn [MOCPP_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MOCPP_MAX_PATH_SIZE, MOCPP_FILENAME_PREFIX "op" "-%u.jsn", opNr);
    if (ret < 0 || ret >= MOCPP_MAX_PATH_SIZE) {
        MOCPP_DBG_ERR("fn error: %i", ret);
        return false;
    }

    DynamicJsonDocument doc {JSON_OBJECT_SIZE(2) + rpc->capacity() + payload->capacity()};
    doc["rpc"] = *rpc;
    doc["payload"] = *payload;

    if (!FilesystemUtils::storeJson(filesystem, fn, doc)) {
        MOCPP_DBG_ERR("FS error");
        return false;
    }

    isPersistent = true;
    return true;
}

bool StoredOperationHandler::restore(unsigned int opNrToLoad) {
    if (isPersistent) {
        MOCPP_DBG_ERR("cannot restore after commit");
        return false;
    }
    if (!filesystem) {
        MOCPP_DBG_DEBUG("filesystem");
        return false;
    }

    opNr = opNrToLoad;

    char fn [MOCPP_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MOCPP_MAX_PATH_SIZE, MOCPP_FILENAME_PREFIX "op" "-%u.jsn", opNr);
    if (ret < 0 || ret >= MOCPP_MAX_PATH_SIZE) {
        MOCPP_DBG_ERR("fn error: %i", ret);
        return false;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MOCPP_DBG_VERBOSE("operation %u does not exist", opNr);
        return false;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn);
    if (!doc) {
        MOCPP_DBG_ERR("FS error");
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

RequestStore::RequestStore(std::shared_ptr<FilesystemAdapter> filesystem) : filesystem(filesystem) {
    opBegin = declareConfiguration<int>("MO_opBegin", 0, MOCPP_OPSTORE_FN, false, false, true, false);

    if (!opBegin || *opBegin < 0) {
        MOCPP_DBG_ERR("init failure");
    } else if (filesystem) {
        opEnd = *opBegin;

        unsigned int misses = 0;
        unsigned int i = opEnd;
        while (misses < 3) {
            char fn [MOCPP_MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, MOCPP_MAX_PATH_SIZE, MOCPP_FILENAME_PREFIX "op" "-%u.jsn", i);
            if (ret < 0 || ret >= MOCPP_MAX_PATH_SIZE) {
                MOCPP_DBG_ERR("fn error: %i", ret);
                misses++;
                i = (i + 1) % MOCPP_MAX_OPNR;
                continue;
            }

            size_t msize;
            if (filesystem->stat(fn, &msize) != 0) {
                MOCPP_DBG_DEBUG("operation %u does not exist", i);
                misses++;
                i = (i + 1) % MOCPP_MAX_OPNR;
                continue;
            }

            //file exists
            misses = 0;
            i = (i + 1) % MOCPP_MAX_OPNR;
            opEnd = i;
        }
    }
}

std::unique_ptr<StoredOperationHandler> RequestStore::makeOpHandler() {
    return std::unique_ptr<StoredOperationHandler>(new StoredOperationHandler(*this, filesystem));
}

unsigned int RequestStore::reserveOpNr() {
    MOCPP_DBG_DEBUG("reserved opNr %u", opEnd);
    auto res = opEnd;
    opEnd++;
    opEnd %= MOCPP_MAX_OPNR;
    return res;
}

void RequestStore::advanceOpNr(unsigned int oldOpNr) {
    if (!opBegin || *opBegin < 0) {
        MOCPP_DBG_ERR("init failure");
        return;
    }

    if (oldOpNr != (unsigned int) *opBegin) {
        if ((oldOpNr + MOCPP_MAX_OPNR - (unsigned int) *opBegin) % MOCPP_MAX_OPNR < 100) {
            MOCPP_DBG_ERR("synchronization failure - try to fix");
            (void)0;
        } else {
            MOCPP_DBG_ERR("synchronization failure");
            return;
        }
    }

    unsigned int opNr = (oldOpNr + 1) % MOCPP_MAX_OPNR;

    //delete range [*opBegin ... opNr)

    unsigned int rangeSize = (opNr + MOCPP_MAX_OPNR - (unsigned int) *opBegin) % MOCPP_MAX_OPNR;

    MOCPP_DBG_DEBUG("delete %u operations", rangeSize);

    for (unsigned int i = 0; i < rangeSize; i++) {
        unsigned int op = ((unsigned int) *opBegin + i + MOCPP_MAX_OPNR - MOCPP_OPHISTORY_SIZE) % MOCPP_MAX_OPNR;

        char fn [MOCPP_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, MOCPP_MAX_PATH_SIZE, MOCPP_FILENAME_PREFIX "op" "-%u.jsn", op);
        if (ret < 0 || ret >= MOCPP_MAX_PATH_SIZE) {
            MOCPP_DBG_ERR("fn error: %i", ret);
            break;
        }

        size_t msize;
        if (filesystem->stat(fn, &msize) != 0) {
            MOCPP_DBG_DEBUG("operation %u does not exist", i);
            continue;
        }

        bool success = filesystem->remove(fn);
        if (!success) {
            MOCPP_DBG_ERR("error deleting %s", fn);
            (void)0;
        }
    }

    MOCPP_DBG_DEBUG("advance opBegin: %u", opNr);
    *opBegin = opNr;
    configuration_save();
}

unsigned int RequestStore::getOpBegin() {
    if (!opBegin || *opBegin < 0) {
        MOCPP_DBG_ERR("invalid state");
        return 0;
    }
    return (unsigned int) *opBegin;
}
