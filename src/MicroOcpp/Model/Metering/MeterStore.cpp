// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Core/FilesystemUtils.h>

#include <MicroOcpp/Debug.h>

#include <algorithm>

#define MO_MAX_STOPTXDATA_LEN 4

using namespace MicroOcpp;

TransactionMeterData::TransactionMeterData(unsigned int connectorId, unsigned int txNr, std::shared_ptr<FilesystemAdapter> filesystem)
        : connectorId(connectorId), txNr(txNr), filesystem{filesystem} {
    
    if (!filesystem) {
        MO_DBG_DEBUG("volatile mode");
        (void)0;
    }
}

bool TransactionMeterData::addTxData(std::unique_ptr<MeterValue> mv) {
    if (isFinalized()) {
        MO_DBG_ERR("immutable");
        return false;
    }

    if (!mv) {
        MO_DBG_ERR("null");
        return false;
    }

    if (MO_MAX_STOPTXDATA_LEN <= 0) {
        //txData off
        return true;
    }

    bool replaceLast = mvCount >= MO_MAX_STOPTXDATA_LEN; //txData size exceeded? overwrite last entry instead of appending

    if (filesystem) {

        unsigned int mvIndex = 0;
        if (replaceLast) {
            mvIndex = mvCount - 1;
        } else {
            mvIndex = mvCount ;
        }

        char fn [MO_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "sd" "-%u-%u-%u.jsn", connectorId, txNr, mvIndex);
        if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("fn error: %i", ret);
            return false;
        }

        auto mvDoc = mv->toJson();
        if (!mvDoc) {
            MO_DBG_ERR("MV not ready yet");
            return false;
        }

        if (!FilesystemUtils::storeJson(filesystem, fn, *mvDoc)) {
            MO_DBG_ERR("FS error");
            return false;
        }

        if (!replaceLast) {
            mvCount++;
        }
    }

    if (replaceLast) {
        txData.back() = std::move(mv);
        MO_DBG_DEBUG("updated latest sd");
    } else {
        txData.push_back(std::move(mv));
        MO_DBG_DEBUG("added sd");
    }
    return true;
}

std::vector<std::unique_ptr<MeterValue>> TransactionMeterData::retrieveStopTxData() {
    if (isFinalized()) {
        MO_DBG_ERR("Can only retrieve once");
        return decltype(txData) {};
    }
    finalize();
    MO_DBG_DEBUG("creating sd");
    return std::move(txData);
}

bool TransactionMeterData::restore(MeterValueBuilder& mvBuilder) {
    if (!filesystem) {
        MO_DBG_DEBUG("No FS - nothing to restore");
        return true;
    }

    const unsigned int MISSES_LIMIT = 3;
    unsigned int i = 0;
    unsigned int misses = 0;

    while (misses < MISSES_LIMIT) { //search until region without mvs found
        
        char fn [MO_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "sd" "-%u-%u-%u.jsn", connectorId, txNr, i);
        if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("fn error: %i", ret);
            return false; //all files have same length
        }

        auto doc = FilesystemUtils::loadJson(filesystem, fn);

        if (!doc) {
            misses++;
            i++;
            continue;
        }

        JsonObject mvJson = doc->as<JsonObject>();
        std::unique_ptr<MeterValue> mv = mvBuilder.deserializeSample(mvJson);

        if (!mv) {
            MO_DBG_ERR("Deserialization error");
            misses++;
            i++;
            continue;
        }

        if (txData.size() >= MO_MAX_STOPTXDATA_LEN) {
            MO_DBG_ERR("corrupted memory");
            return false;
        }

        txData.push_back(std::move(mv));

        mvCount = i;
        i++;
        misses = 0;
    }

    MO_DBG_DEBUG("Restored %zu meter values", txData.size());
    return true;
}

MeterStore::MeterStore(std::shared_ptr<FilesystemAdapter> filesystem) : filesystem {filesystem} {

    if (!filesystem) {
        MO_DBG_DEBUG("volatile mode");
        (void)0;
    }
}

std::shared_ptr<TransactionMeterData> MeterStore::getTxMeterData(MeterValueBuilder& mvBuilder, Transaction *transaction) {
    if (!transaction || transaction->isSilent()) {
        //no tx assignment -> don't store txData
        //tx is silent -> no StopTx will be sent and don't store txData
        return nullptr;
    }
    auto connectorId = transaction->getConnectorId();
    auto txNr = transaction->getTxNr();
    
    auto cached = std::find_if(txMeterData.begin(), txMeterData.end(),
            [connectorId, txNr] (std::weak_ptr<TransactionMeterData>& txm) {
                if (auto txml = txm.lock()) {
                    return txml->getConnectorId() == connectorId && txml->getTxNr() == txNr;
                } else {
                    return false;
                }
            });
    
    if (cached != txMeterData.end()) {
        if (auto cachedl = cached->lock()) {
            return cachedl;
        }
    }

    //clean outdated pointers before creating new object

    txMeterData.erase(std::remove_if(txMeterData.begin(), txMeterData.end(),
            [] (std::weak_ptr<TransactionMeterData>& txm) {
                return txm.expired();
            }),
            txMeterData.end());

    //create new object and cache weak pointer

    auto tx = std::make_shared<TransactionMeterData>(connectorId, txNr, filesystem);
    
    if (filesystem) {
        char fn [MO_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "sd" "-%u-%u-%u.jsn", connectorId, txNr, 0);
        if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("fn error: %i", ret);
            return nullptr; //cannot store
        }

        size_t size = 0;
        bool exists = filesystem->stat(fn, &size) == 0;

        if (exists) {
            if (!tx->restore(mvBuilder)) {
                remove(connectorId, txNr);
                MO_DBG_ERR("removed corrupted tx entries");
            }
        }
    }

    txMeterData.push_back(tx);

    MO_DBG_DEBUG("Added txNr %u, now holding %zu txs", txNr, txMeterData.size());

    return tx;
}

bool MeterStore::remove(unsigned int connectorId, unsigned int txNr) {

    unsigned int mvCount = 0;

    auto cached = std::find_if(txMeterData.begin(), txMeterData.end(),
            [connectorId, txNr] (std::weak_ptr<TransactionMeterData>& txm) {
                if (auto txml = txm.lock()) {
                    return txml->getConnectorId() == connectorId && txml->getTxNr() == txNr;
                } else {
                    return false;
                }
            });
    
    if (cached != txMeterData.end()) {
        if (auto cachedl = cached->lock()) {
            mvCount = cachedl->getPathsCount();
            cachedl->finalize();
        }
    }

    bool success = true;

    if (filesystem) {
        if (mvCount == 0) {
            const unsigned int MISSES_LIMIT = 3;
            unsigned int misses = 0;
            unsigned int i = 0;

            while (misses < MISSES_LIMIT) { //search until region without mvs found
                
                char fn [MO_MAX_PATH_SIZE] = {'\0'};
                auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "sd" "-%u-%u-%u.jsn", connectorId, txNr, i);
                if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
                    MO_DBG_ERR("fn error: %i", ret);
                    return false; //all files have same length
                }

                size_t nsize = 0;
                if (filesystem->stat(fn, &nsize) != 0) {
                    misses++;
                    i++;
                    continue;
                }

                i++;
                mvCount = i;
                misses = 0;
            }
        }

        MO_DBG_DEBUG("remove %u mvs for txNr %u", mvCount, txNr);

        for (unsigned int i = 0; i < mvCount; i++) {
            unsigned int sd = mvCount - 1U - i;
        
            char fn [MO_MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "sd" "-%u-%u-%u.jsn", connectorId, txNr, sd);
            if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
                MO_DBG_ERR("fn error: %i", ret);
                return false;
            }

            success &= filesystem->remove(fn);
        }
    }

    //clean outdated pointers

    txMeterData.erase(std::remove_if(txMeterData.begin(), txMeterData.end(),
            [] (std::weak_ptr<TransactionMeterData>& txm) {
                return txm.expired();
            }),
            txMeterData.end());

    if (success) {
        MO_DBG_DEBUG("Removed meter values for cId %u, txNr %u", connectorId, txNr);
        (void)0;
    } else {
        MO_DBG_DEBUG("corrupted fs");
    }

    return success;
}
