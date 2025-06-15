// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Metering/MeterStore.h>

#include <algorithm>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>


#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

bool MeterStore::printTxMeterValueFName(char *fname, size_t size, unsigned int evseId, unsigned int txNr, unsigned int mvIndex) {
    auto ret = snprintf(fname, MO_MAX_PATH_SIZE, "sd-%.*u-%.*u-%.*u.jsn",
        MO_NUM_EVSEID_DIGITS, evseId, MO_TXNR_DIGITS, txNr, MO_STOPTXDATA_DIGITS, mvIndex);
    if (ret < 0 || (size_t)ret >= size) {
        MO_DBG_ERR("fname error: %i", ret);
        return false;
    }
    return true;
}

FilesystemUtils::LoadStatus MeterStore::load(MO_FilesystemAdapter *filesystem, Context& context, Vector<MO_MeterInput>& meterInputs, unsigned int evseId, unsigned int txNr, Vector<MeterValue*>& txMeterValues) {

    auto& clock = context.getClock();

    for (unsigned int i = 0; i < MO_STOPTXDATA_MAX_SIZE; i++) { //search until region without mvs found

        char fname [MO_MAX_PATH_SIZE] = {'\0'};
        if (!printTxMeterValueFName(fname, sizeof(fname), evseId, txNr, i)) {
            MO_DBG_ERR("fname error %u %u", evseId, txNr);
            return FilesystemUtils::LoadStatus::ErrOther;
        }

        JsonDoc doc (0);

        auto ret = FilesystemUtils::loadJson(filesystem, fname, doc, "v16/v201.Metering.MeterStore");
        if (ret == FilesystemUtils::LoadStatus::FileNotFound) {
            break; //done loading meterValues
        }
        if (ret != FilesystemUtils::LoadStatus::Success) {
            return ret;
        }

        auto capacity = txMeterValues.size() + 1;
        txMeterValues.reserve(capacity);
        if (txMeterValues.capacity() < capacity) {
            MO_DBG_ERR("OOM");
            return FilesystemUtils::LoadStatus::ErrOOM;
        }

        auto mv = new MeterValue();
        if (!mv) {
            MO_DBG_ERR("OOM");
            return FilesystemUtils::LoadStatus::ErrOOM;
        }
        txMeterValues.push_back(mv);

        if (!mv->parseJson(clock, meterInputs, doc.as<JsonObject>())) {
            MO_DBG_ERR("deserialization error %s", fname);
            return FilesystemUtils::LoadStatus::ErrFileCorruption;
        }
    }

    //success

    MO_DBG_DEBUG("Restored tx %u-%u with %zu meter values", evseId, txNr, txMeterValues.size());

    if (txMeterValues.empty()) {
        return FilesystemUtils::LoadStatus::FileNotFound;
    }

    return FilesystemUtils::LoadStatus::Success;
}

FilesystemUtils::StoreStatus MeterStore::store(MO_FilesystemAdapter *filesystem, Context& context, unsigned int evseId, unsigned int txNr, unsigned int mvIndex, MeterValue& mv) {

    char fname [MO_MAX_PATH_SIZE];
    if (!printTxMeterValueFName(fname, sizeof(fname), evseId, txNr, mvIndex)) {
        MO_DBG_ERR("fname error %u %u %u", evseId, txNr, mvIndex);
        return FilesystemUtils::StoreStatus::ErrOther;
    }

    auto& clock = context.getClock();

    JsonDoc doc = initJsonDoc("v16/v201.MeterStore", 1024);
    if (!mv.toJson(clock, context.getOcppVersion(), /*internal format*/ true, doc.as<JsonObject>())) {
        MO_DBG_ERR("serialization error %s", fname);
        return FilesystemUtils::StoreStatus::ErrJsonCorruption;
    }

    auto ret = FilesystemUtils::storeJson(filesystem, fname, doc);
    if (ret != FilesystemUtils::StoreStatus::Success) {
        MO_DBG_ERR("fs error %s %i", fname, (int)ret);
    }

    return ret;
}

bool MeterStore::remove(MO_FilesystemAdapter *filesystem, unsigned int evseId, unsigned int txNr) {

    char fname [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fname, MO_MAX_PATH_SIZE, "sd-%.*u-%.*u-",
        MO_NUM_EVSEID_DIGITS, evseId, MO_TXNR_DIGITS, txNr);
    if (ret < 0 || (size_t)ret >= sizeof(fname)) {
        MO_DBG_ERR("fname error: %i", ret);
        return false;
    }

    return FilesystemUtils::removeByPrefix(filesystem, fname);
}

#endif //MO_ENABLE_V16
