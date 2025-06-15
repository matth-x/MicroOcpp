// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <limits>

#include <MicroOcpp/Core/PersistencyUtils.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

bool PersistencyUtils::loadBootStats(MO_FilesystemAdapter *filesystem, BootStats& bstats) {

    JsonDoc json {0};
    bool success = false;

    auto ret = FilesystemUtils::loadJson(filesystem, "bootstats.jsn", json, "PersistencyUtils");
    switch (ret) {
        case FilesystemUtils::LoadStatus::Success: {
            success = true;

            int bootNrIn = json["bootNr"] | 0;
            if (bootNrIn >= 0 && bootNrIn <= std::numeric_limits<uint16_t>::max()) {
                bstats.bootNr = (uint16_t)bootNrIn;
            } else {
                success = false;
            }

            int attemptsIn = json["attempts"] | -1;
            if (attemptsIn >= 0 && attemptsIn <= std::numeric_limits<uint16_t>::max()) {
                bstats.attempts = (uint16_t) attemptsIn;
            } //else: attempts counter can be missing after upgrade from pre 2.0.0 version

            const char *microOcppVersionIn = json["MicroOcppVersion"] | (const char*)nullptr;
            if (microOcppVersionIn) {
                auto ret = snprintf(bstats.microOcppVersion, sizeof(bstats.microOcppVersion), "%s", microOcppVersionIn);
                if (ret < 0 || (size_t)ret >= sizeof(bstats.microOcppVersion)) {
                    success = false;
                }
            } //else: version specifier can be missing after upgrade from pre 1.2.0 version

            if (!success) {
                MO_DBG_ERR("bootstats invalid data");
                bstats = BootStats();
            }
            break; }
        case FilesystemUtils::LoadStatus::FileNotFound:
            MO_DBG_DEBUG("populate bootstats");
            success = true;
            break;
        case FilesystemUtils::LoadStatus::ErrOOM:
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("error loading bootstats %i", (int)ret);
            success = false;
            break;
    }

    return success;
}

bool PersistencyUtils::storeBootStats(MO_FilesystemAdapter *filesystem, BootStats& bstats) {

    auto json = initJsonDoc("PersistencyUtils", JSON_OBJECT_SIZE(3));

    json["bootNr"] = bstats.bootNr;
    json["attempts"] = bstats.attempts;
    json["MicroOcppVersion"] = (const char*)bstats.microOcppVersion;

    auto ret = FilesystemUtils::storeJson(filesystem, "bootstats.jsn", json);
    bool success = (ret == FilesystemUtils::StoreStatus::Success);

    if (!success) {
        MO_DBG_ERR("error storing bootstats %i", (int)ret);
    }

    return success;
}

bool PersistencyUtils::migrate(MO_FilesystemAdapter *filesystem) {

    BootStats bstats;
    if (!loadBootStats(filesystem, bstats)) {
        return false;
    }

    bool success = true;

    if (strcmp(bstats.microOcppVersion, MO_VERSION)) {
        MO_DBG_INFO("migrate persistent storage to MO v" MO_VERSION);
        success &= FilesystemUtils::removeByPrefix(filesystem, "sd");
        success &= FilesystemUtils::removeByPrefix(filesystem, "tx");
        success &= FilesystemUtils::removeByPrefix(filesystem, "op");
        success &= FilesystemUtils::removeByPrefix(filesystem, "sc-");
        success &= FilesystemUtils::removeByPrefix(filesystem, "client-state.cnf");
        success &= FilesystemUtils::removeByPrefix(filesystem, "arduino-ocpp.cnf");
        success &= FilesystemUtils::removeByPrefix(filesystem, "ocpp-creds.jsn");

        snprintf(bstats.microOcppVersion, sizeof(bstats.microOcppVersion), "%s", MO_VERSION);
        MO_DBG_DEBUG("clear local state files (migration): %s", success ? "success" : "not completed");

        success &= storeBootStats(filesystem, bstats);
    }
    return success;
}

bool PersistencyUtils::autoRecovery(MO_FilesystemAdapter *filesystem) {

    BootStats bstats;
    if (!loadBootStats(filesystem, bstats)) {
        return false;
    }

    bstats.attempts++;

    bool success = storeBootStats(filesystem, bstats);

    if (bstats.attempts <= 3) {
        //no boot loop - just increment attempts and we're good
        return success;
    }

    bool ret = true;
    ret &= FilesystemUtils::removeByPrefix(filesystem, "sd");
    ret &= FilesystemUtils::removeByPrefix(filesystem, "tx");
    ret &= FilesystemUtils::removeByPrefix(filesystem, "sc-");
    ret &= FilesystemUtils::removeByPrefix(filesystem, "reservation");
    ret &= FilesystemUtils::removeByPrefix(filesystem, "client-state");

    MO_DBG_ERR("clear local state files (recovery): %s", ret ? "success" : "not completed");
    success &= ret;

    return success;
}

bool PersistencyUtils::setBootSuccess(MO_FilesystemAdapter *filesystem) {

    BootStats bstats;
    if (!loadBootStats(filesystem, bstats)) {
        return false;
    }

    if (bstats.attempts == 0) {
        // attempts already 0 - no need to write back file
        return true;
    }

    bstats.attempts = 0;

    return storeBootStats(filesystem, bstats);
}
