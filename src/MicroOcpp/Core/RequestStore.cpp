// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/RequestStore.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

#define MO_OPSTORE_FN MO_FILENAME_PREFIX "opstore.jsn"

#define MO_OPHISTORY_SIZE 3

using namespace MicroOcpp;

bool StoredOperationHandler::commit() {
    if (isPersistent) {
        MO_DBG_ERR("cannot call two times");
        return false;
    }
    if (!filesystem) {
        MO_DBG_DEBUG("filesystem");
        return false;
    }

    if (!rpc || !payload) {
        MO_DBG_ERR("unitialized");
        return false;
    }

    opNr = context.reserveOpNr();

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "op" "-%u.jsn", opNr);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    DynamicJsonDocument doc {JSON_OBJECT_SIZE(2) + rpc->capacity() + payload->capacity()};
    doc["rpc"] = *rpc;
    doc["payload"] = *payload;

    if (!FilesystemUtils::storeJson(filesystem, fn, doc)) {
        MO_DBG_ERR("FS error");
        return false;
    }

    isPersistent = true;
    return true;
}

bool StoredOperationHandler::restore(unsigned int opNrToLoad) {
    if (isPersistent) {
        MO_DBG_ERR("cannot restore after commit");
        return false;
    }
    if (!filesystem) {
        MO_DBG_DEBUG("filesystem");
        return false;
    }

    opNr = opNrToLoad;

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "op" "-%u.jsn", opNr);
    if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    size_t msize;
    if (filesystem->stat(fn, &msize) != 0) {
        MO_DBG_VERBOSE("operation %u does not exist", opNr);
        return false;
    }

    auto doc = FilesystemUtils::loadJson(filesystem, fn);
    if (!doc) {
        MO_DBG_ERR("FS error");
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
    opBeginInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "opBegin", 0, MO_OPSTORE_FN, false, false, false);
    configuration_load(MO_OPSTORE_FN);

    if (!opBeginInt || opBeginInt->getInt() < 0) {
        MO_DBG_ERR("init failure");
    } else if (filesystem) {
        opEnd = opBeginInt->getInt();

        unsigned int misses = 0;
        unsigned int i = opEnd;
        while (misses < 3) {
            char fn [MO_MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "op" "-%u.jsn", i);
            if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
                MO_DBG_ERR("fn error: %i", ret);
                misses++;
                i = (i + 1) % MO_MAX_OPNR;
                continue;
            }

            size_t msize;
            if (filesystem->stat(fn, &msize) != 0) {
                MO_DBG_DEBUG("operation %u does not exist", i);
                misses++;
                i = (i + 1) % MO_MAX_OPNR;
                continue;
            }

            //file exists
            misses = 0;
            i = (i + 1) % MO_MAX_OPNR;
            opEnd = i;
        }
    }
}

std::unique_ptr<StoredOperationHandler> RequestStore::makeOpHandler() {
    return std::unique_ptr<StoredOperationHandler>(new StoredOperationHandler(*this, filesystem));
}

unsigned int RequestStore::reserveOpNr() {
    MO_DBG_DEBUG("reserved opNr %u", opEnd);
    auto res = opEnd;
    opEnd++;
    opEnd %= MO_MAX_OPNR;
    return res;
}

void RequestStore::advanceOpNr(unsigned int oldOpNr) {
    if (!opBeginInt || opBeginInt->getInt() < 0) {
        MO_DBG_ERR("init failure");
        return;
    }

    if (oldOpNr != (unsigned int) opBeginInt->getInt()) {
        if ((oldOpNr + MO_MAX_OPNR - (unsigned int) opBeginInt->getInt()) % MO_MAX_OPNR < 100) {
            MO_DBG_ERR("synchronization failure - try to fix");
            (void)0;
        } else {
            MO_DBG_ERR("synchronization failure");
            return;
        }
    }

    unsigned int opNr = (oldOpNr + 1) % MO_MAX_OPNR;

    //delete range [opBeginInt->getInt() ... opNr)

    unsigned int rangeSize = (opNr + MO_MAX_OPNR - (unsigned int) opBeginInt->getInt()) % MO_MAX_OPNR;

    MO_DBG_DEBUG("delete %u operations", rangeSize);

    for (unsigned int i = 0; i < rangeSize; i++) {
        unsigned int op = ((unsigned int) opBeginInt->getInt() + i + MO_MAX_OPNR - MO_OPHISTORY_SIZE) % MO_MAX_OPNR;

        char fn [MO_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "op" "-%u.jsn", op);
        if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("fn error: %i", ret);
            break;
        }

        size_t msize;
        if (filesystem->stat(fn, &msize) != 0) {
            MO_DBG_DEBUG("operation %u does not exist", i);
            continue;
        }

        bool success = filesystem->remove(fn);
        if (!success) {
            MO_DBG_ERR("error deleting %s", fn);
            (void)0;
        }
    }

    MO_DBG_DEBUG("advance opBegin: %u", opNr);
    opBeginInt->setInt(opNr);
    configuration_save();
}

unsigned int RequestStore::getOpBegin() {
    if (!opBeginInt || opBeginInt->getInt() < 0) {
        MO_DBG_ERR("invalid state");
        return 0;
    }
    return (unsigned int) opBeginInt->getInt();
}
