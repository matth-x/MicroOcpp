// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Model/SecurityEvent/SecurityEventService.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Operations/GetLog.h>
#include <MicroOcpp/Operations/SecurityEventNotification.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT

/* Queue naming schema
 *
 * The SecurityEvent queue keeps historic events for the GetLogs operation / later troubleshooting and it keeps
 * outstanding events in case the charger is offline.
 *
 * To store the queue on the file system, it is partitioned into fewer, but larger files. Most filesystems come with
 * an overhead if storing small, but many files. So MO uses one file to store up to 20 entries. This results in a
 * two-dimensional index to address a SecurityEvent: first the position of the file on the filesystem and second the
 * position of the SecurityEvent in the file. So a SecurityEvent is addressed by fileNr x entryNr.
 *
 * The SecurityEvent queue the queue consists of two sections, the historic section, followed by the outstanding
 * section. The first element of the historic section is the "begin", the first of the outstanding is the "front",
 * and the outstanding section is delimited by the "end" index which is one address higher than the last element.
 * This results in the following arithmetics:
 * 
 * front - begin: number of historic elements
 * front == begin: true if no historic elements exist
 * end - front: number of outstanding elements to be sent
 * end == front: true if no outstanding elements exist
 * end - begin: number of total elements on flash
 * end == front: true if there are no files on flash
 *
 * The fileNr is a ring index which rolls over at MO_SECLOG_INDEX_MAX. Going to the next file from index
 * (MO_SECLOG_INDEX_MAX - 1) results in the index 0. This breaks the < and > comparators, because the previous
 * logfile index may be numerically higher than the following logfile index. This is handled by some explicit
 * modulus operations. The other index, entryNr, is an ordinary index though and does not need any of the ring
 * index arithmetics.
 */

using namespace MicroOcpp;

SecurityEventService::SecurityEventService(Context& context) : MemoryManaged("v201.Security.SecurityEventService"), context(context) {

}

bool SecurityEventService::setup() {

    ocppVersion = context.getOcppVersion();

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {

        auto configService = context.getModel16().getConfigurationService();
        if (!configService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        //if transactions can start before the BootNotification succeeds
        timeAdjustmentReportingThresholdIntV16 = configService->declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "TimeAdjustmentReportingThreshold", 20);
        if (!timeAdjustmentReportingThresholdIntV16) {
            MO_DBG_ERR("initialization error");
            return false;
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {

        auto varService = context.getModel201().getVariableService();
        if (!varService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        timeAdjustmentReportingThresholdIntV201 = varService->declareVariable<int>("ClockCtrlr", "TimeAdjustmentReportingThreshold", 20);
        if (!timeAdjustmentReportingThresholdIntV201) {
            return false;
        }
    }
    #endif //MO_ENABLE_V201

    auto& clock = context.getClock();
    trackUnixTime = clock.now();
    trackUptime = clock.getUptime();

    connection = context.getConnection();
    if (!connection) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("volatile mode");
    }

    if (filesystem) {
        if (!FilesystemUtils::loadRingIndex(filesystem, MO_SECLOG_FN_PREFIX, MO_SECLOG_INDEX_MAX, &fileNrBegin, &fileNrEnd)) {
            MO_DBG_ERR("failed to init security event log");
            return false;
        }

        // find front file and front securityEvent inside that file
        fileNrFront = fileNrEnd;
        unsigned int fileNrSize = (fileNrEnd + MO_SECLOG_INDEX_MAX - fileNrBegin) % MO_SECLOG_INDEX_MAX;

        for (unsigned int i = 1; i <= fileNrSize; i++) {

            unsigned int fileNr = (fileNrEnd + MO_SECLOG_INDEX_MAX - i) % MO_SECLOG_INDEX_MAX; // iterate logFiles from back

            char fn [MO_MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, sizeof(fn), MO_SECLOG_FN_PREFIX "%u.jsn", fileNr);
            if (ret < 0 || (size_t)ret >= sizeof(fn)) {
                MO_DBG_ERR("fn error: %i", ret);
                return false;
            }

            JsonDoc logFile (0);
            auto status = FilesystemUtils::loadJson(filesystem, fn, logFile, getMemoryTag());
            switch (status) {
                case FilesystemUtils::LoadStatus::Success:
                    break; //continue loading JSON
                case FilesystemUtils::LoadStatus::FileNotFound:
                    break; //file gap - will skip this fileNr
                case FilesystemUtils::LoadStatus::ErrOOM:
                    MO_DBG_ERR("OOM");
                    return false;
                case FilesystemUtils::LoadStatus::ErrFileCorruption:
                case FilesystemUtils::LoadStatus::ErrOther:
                    MO_DBG_ERR("failed to load %s", fn);
                    break;
            }

            if (status != FilesystemUtils::LoadStatus::Success) {
                continue; 
            }

            unsigned int nextEntryNrFront = (unsigned int)logFile.size();
            unsigned int nextEntryNrEnd = (unsigned int)logFile.size();

            for (size_t entryNr = 0; entryNr < logFile.size(); entryNr++) {
                auto securityEvent = logFile[entryNr];
                if (securityEvent.containsKey("atnr")) {
                    // this is the first entry to be sent. Found front candidate
                    nextEntryNrFront = (unsigned int)entryNr;
                    break;
                }
            }

            if (nextEntryNrFront != nextEntryNrEnd) {
                // This file contains outstanding SecurityEvents to be sent. These could be the front of the queue
                fileNrFront = fileNr;
                entryNrFront = nextEntryNrFront;
                entryNrEnd = nextEntryNrEnd;
            } else {
                // This file does not contain any events to send. So this loop iteration already skipped over the front file
                break;
            }

            if (nextEntryNrFront > 0) {
                // This file contains both historic and outstanding events. It is safe to say that this is the front and to stop the search
                break;
            }

            // continue: This file contains only events to send and no historic events. This could be the front file
            // or in the middle of the queue. Took it as candidate, but continue searching at fileNr-1
        }
    }

    return true;
}

void SecurityEventService::loop() {

    auto& clock = context.getClock();
    Timestamp unixTime = clock.now();
    Timestamp uptime = clock.getUptime();

    if (unixTime.isUnixTime() && trackUnixTime.isUnixTime()) {
        // Got initial time - check if clock drift exceeds timeAdjustmentReportingThreshold
        
        int32_t deltaUnixTime = 0; //unix time jumps over clock adjustments
        clock.delta(unixTime, trackUnixTime, deltaUnixTime);

        int32_t deltaUptime = 0; //uptime is steadily increasing without gaps
        clock.delta(uptime, trackUptime, deltaUptime);

        int32_t timeAdjustment = deltaUnixTime - deltaUptime; //should be 0 if clock is not adjusted, otherwise the number of seconds adjusted

        int timeAdjustmentReportingThreshold = 0;
        #if MO_ENABLE_V16
        if (ocppVersion == MO_OCPP_V16) {
            timeAdjustmentReportingThreshold = timeAdjustmentReportingThresholdIntV16->getInt();
        }
        #endif //MO_ENABLE_V16
        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            timeAdjustmentReportingThreshold = timeAdjustmentReportingThresholdIntV201->getInt();
        }
        #endif //MO_ENABLE_V201

        if (abs(timeAdjustment) >= timeAdjustmentReportingThreshold && timeAdjustmentReportingThreshold > 0) {
            triggerSecurityEvent("SettingSystemTime");
        }
    }
    trackUnixTime = unixTime;
    trackUptime = uptime;

    if (!filesystem) {
        // Nothing else to do in volatile mode
        return;
    }

    if (isSecurityEventInProgress) {
        //avoid sending multiple messages at the same time
        return;
    }

    if (!connection->isConnected()) {
        //don't use up attempts while offline
        return;
    }

    int32_t dtAttemptTime;
    if (!attemptTime.isDefined() || !context.getClock().delta(context.getClock().now(), attemptTime, dtAttemptTime)) {
        dtAttemptTime = 0;
    }

    if (entryNrFront != entryNrEnd &&
        dtAttemptTime >= (int32_t)attemptNr * MO_SECLOG_SEND_ATTEMPT_INTERVAL) {

        // Send SecurityEventNotification

        attemptTime = context.getClock().now();

        char type [MO_SECURITY_EVENT_TYPE_LENGTH + 1];
        Timestamp timestamp;
        unsigned int attemptNr;

        bool success = fetchSecurityEventFront(type, timestamp, attemptNr);
        if (!success) {
            return; //retry
        }

        this->attemptNr = attemptNr;

        auto securityEventNotification = makeRequest(context, new SecurityEventNotification(context, type, timestamp));
        securityEventNotification->setOnReceiveConf([this] (JsonObject) {
            isSecurityEventInProgress = false;

            MO_DBG_DEBUG("completed front securityEvent");
            advanceSecurityEventFront();
        });
        securityEventNotification->setOnAbort([this] () {
            isSecurityEventInProgress = false;

            if (this->attemptNr >= MO_SECLOG_SEND_ATTEMPTS) {
                MO_DBG_ERR("final SecurityEventNotification attempt failed");
                advanceSecurityEventFront();
            } else {
                MO_DBG_DEBUG("retry SecurityEventNotification");
            }
        });
        isSecurityEventInProgress = true;

        securityEventNotification->setTimeout(std::min(20, MO_SECLOG_SEND_ATTEMPT_INTERVAL));
        context.getMessageService().sendRequest(std::move(securityEventNotification));
    }
}

bool SecurityEventService::fetchSecurityEventFront(char *type, Timestamp& timestamp, unsigned int& attemptNr) {
    if (!filesystem) {
        MO_DBG_ERR("invalid state");
        return false;
    }

    if (fileNrFront == fileNrEnd || entryNrFront == entryNrEnd) {
        MO_DBG_ERR("invalid state");
        return false;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, sizeof(fn), MO_SECLOG_FN_PREFIX "%u.jsn", fileNrFront);
    if (ret < 0 || (size_t)ret >= sizeof(fn)) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    JsonDoc logFile (0);
    auto status = FilesystemUtils::loadJson(filesystem, fn, logFile, getMemoryTag());
    switch (status) {
        case FilesystemUtils::LoadStatus::Success:
            break; //continue loading JSON
        case FilesystemUtils::LoadStatus::FileNotFound:
            advanceSecurityEventFront();
            break; //file gap - will skip this fileNr
        case FilesystemUtils::LoadStatus::ErrOOM:
            MO_DBG_ERR("OOM");
            break;
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
            MO_DBG_ERR("failed to load %s", fn);
            advanceSecurityEventFront();
            break;
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("failed to load %s", fn);
            break;
    }

    if (status != FilesystemUtils::LoadStatus::Success) {
        return false;
    }

    if (logFile.size() != (size_t)entryNrEnd) {
        MO_DBG_ERR("deserialization error: %s", fn);
        advanceSecurityEventFront();
        return false;
    }

    JsonObject securityEvent = logFile[entryNrFront];

    const char *typeIn = securityEvent["type"] | (const char*)nullptr;
    ret = -1;
    if (typeIn) {
        ret = snprintf(type, MO_SECURITY_EVENT_TYPE_LENGTH + 1, "%s", typeIn);
    }
    if (!typeIn || ret < 0 || ret >= MO_SECURITY_EVENT_TYPE_LENGTH) {
        MO_DBG_ERR("deserialization error");
        advanceSecurityEventFront();
        return false;
    }

    const char *time = securityEvent["time"] | (const char*)nullptr;
    if (!time || !context.getClock().parseString(time, timestamp)) {
        MO_DBG_ERR("deserialization error");
        advanceSecurityEventFront();
        return false;
    }

    attemptNr = securityEvent["atnr"] | MO_SECLOG_SEND_ATTEMPTS;
    if (attemptNr >= MO_SECLOG_SEND_ATTEMPTS) {
        MO_DBG_ERR("invalid attemptNr");
        return false;
    }

    attemptNr++;

    if (attemptNr < MO_SECLOG_SEND_ATTEMPTS) {
        securityEvent["atnr"] = attemptNr;
    } else {
        securityEvent.remove("atnr");
    }

    if (FilesystemUtils::storeJson(filesystem, fn, logFile) != FilesystemUtils::StoreStatus::Success) {
        MO_DBG_ERR("FS error: %s", fn);
        return false;
    }

    return true;
}

bool SecurityEventService::advanceSecurityEventFront() {

    if (!filesystem) {
        MO_DBG_ERR("invalid state");
        return false;
    }

    // this invalidates attemptTime of previous securityEvent
    attemptTime = Timestamp();
    attemptNr = 0;

    // Advance entryNrFront
    entryNrFront++;

    if (entryNrFront != entryNrEnd) {
        // This file has further securityEvents. Done here
        return true;
    }

    // Current front file done. Advance front file
    fileNrFront = (fileNrFront + 1) % MO_SECLOG_INDEX_MAX;
    entryNrFront = 0;
    entryNrEnd = 0;

    if (fileNrFront == fileNrEnd) {
        // All logs sent. Done here
        return true;
    }

    // Check if new front logfile already contains entries and if so, update entryNrEnd. Skip corrupt files if necessary

    for (; fileNrFront != fileNrEnd; fileNrFront = (fileNrFront + 1) % MO_SECLOG_INDEX_MAX) {

        char fn [MO_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, sizeof(fn), MO_SECLOG_FN_PREFIX "%u.jsn", fileNrFront);
        if (ret < 0 || (size_t)ret >= sizeof(fn)) {
            MO_DBG_ERR("fn error: %i", ret);
            return false;
        }

        JsonDoc logFile (0);
        auto status = FilesystemUtils::loadJson(filesystem, fn, logFile, getMemoryTag());
        switch (status) {
            case FilesystemUtils::LoadStatus::Success:
                break; //continue loading JSON
            case FilesystemUtils::LoadStatus::FileNotFound:
                break; //file gap - will skip this fileNr
            case FilesystemUtils::LoadStatus::ErrOOM:
                MO_DBG_ERR("OOM");
                return false;
            case FilesystemUtils::LoadStatus::ErrFileCorruption:
            case FilesystemUtils::LoadStatus::ErrOther:
                MO_DBG_ERR("failure to load %s. Skipping logfile", fn);
                break;
        }

        if (status != FilesystemUtils::LoadStatus::Success) {
            continue;
        }
    
        if (logFile.size() == 0) {
            MO_DBG_ERR("invalid state: %s. Skpping logfile", fn);
            continue;
        }
    
        unsigned int nextEntryNrFront = (unsigned int)logFile.size();
        unsigned int nextEntryNrEnd = (unsigned int)logFile.size();

        for (size_t entryNr = 0; entryNr < logFile.size(); entryNr++) {
            auto securityEvent = logFile[entryNr];
            if (securityEvent.containsKey("atnr")) {
                // this is the first entry to be sent. Found front
                nextEntryNrFront = (unsigned int)entryNr;
                break;
            }
        }
    
        if (nextEntryNrFront == nextEntryNrEnd) {
            MO_DBG_ERR("invalid state: %s. Skpping logfile", fn);
            continue;
        }

        // logfile is valid. Apply Nrs and exit for-loop
        entryNrFront = nextEntryNrFront;
        entryNrEnd = nextEntryNrEnd;
        break;
    }

    // Advanced to the next front SecurityEvent
    return true;
}

bool SecurityEventService::triggerSecurityEvent(const char *eventType) {

    if (!filesystem) {
        // Volatile mode. Just enqueue SecurityEventNotification to OCPP message queue and return
        auto securityEventNotification = makeRequest(context, new SecurityEventNotification(context, eventType, context.getClock().now()));
        context.getMessageService().sendRequest(std::move(securityEventNotification));
        return true;
    }

    // Append SecurityEvent to queue

    unsigned int fileNrBack = fileNrEnd;
    JsonDoc logBack (0);
    bool logBackLoaded = false;

    if (fileNrBegin != fileNrEnd) {

        unsigned int fileNrBack = (fileNrEnd + MO_SECLOG_INDEX_MAX - 1) % MO_SECLOG_INDEX_MAX;

        char fn [MO_MAX_PATH_SIZE] = {'\0'};
        auto ret = snprintf(fn, sizeof(fn), MO_SECLOG_FN_PREFIX "%u.jsn", fileNrBack);
        if (ret < 0 || (size_t)ret >= sizeof(fn)) {
            MO_DBG_ERR("fn error: %i", ret);
            return false;
        }

        auto status = FilesystemUtils::loadJson(filesystem, fn, logBack, getMemoryTag());
        switch (status) {
            case FilesystemUtils::LoadStatus::Success:
                logBackLoaded = true;
                break; //continue loading JSON
            case FilesystemUtils::LoadStatus::FileNotFound:
                break; //file gap - will skip this fileNr
            case FilesystemUtils::LoadStatus::ErrOOM:
                MO_DBG_ERR("OOM");
                return false;
            case FilesystemUtils::LoadStatus::ErrFileCorruption:
            case FilesystemUtils::LoadStatus::ErrOther:
                MO_DBG_ERR("failure to load %s", fn);
                break;
        }

        if (!logBackLoaded || logBack.size() == 0) { // Is logBack errorneous? (load failure or empty JSON)
            MO_DBG_ERR("invalid state: %s. Skpping logfile", fn);
            fileNrBack = fileNrEnd;
            logBack.clear();
            logBackLoaded = false;
        } else if (logBack.size() >= MO_SECLOG_MAX_FILE_ENTRIES) { // Is logBack aready full?
            // Yes, start new logfile
            fileNrBack = fileNrEnd;
            logBack.clear();
            logBackLoaded = false;
        }
    }

    // Is security log full?
    unsigned int outstandingFiles = (fileNrBack + MO_SECLOG_INDEX_MAX - fileNrFront) % MO_SECLOG_INDEX_MAX;
    if (outstandingFiles > MO_SECLOG_MAX_FILES) {
        MO_DBG_ERR("SecurityLog capacity exceeded");
        return false;
    }

    // Are there historic logfiles which should be deleted?
    unsigned int historicFiles = (fileNrFront + MO_SECLOG_INDEX_MAX - fileNrBegin) % MO_SECLOG_INDEX_MAX;
    if (historicFiles + outstandingFiles > MO_SECLOG_MAX_FILES) {
        MO_DBG_ERR("Cleaning historic logfile");
        
        char fn [MO_MAX_PATH_SIZE];
        auto ret = snprintf(fn, sizeof(fn), MO_SECLOG_FN_PREFIX "%u.jsn", fileNrBegin);
        if (ret < 0 || (size_t)ret >= sizeof(fn)) {
            MO_DBG_ERR("fn error: %i", ret);
            return false;
        }

        char path [MO_MAX_PATH_SIZE];
        if (!FilesystemUtils::printPath(filesystem, path, sizeof(path), fn)) {
            MO_DBG_ERR("fn error: %i", ret);
            return false;
        }

        if (!filesystem->remove(path)) {
            MO_DBG_ERR("failure to remove %s", path);
            return false;
        }

        // Advance fileNrBegin
        fileNrBegin = (fileNrBegin + 1) % MO_SECLOG_INDEX_MAX;
    }

    // Determine JSON capacity

    size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(3);

    if (logBackLoaded) {
        // Keep exisitng logfile contents - therefore need enough space for copying everything over to next write
        capacity += JSON_ARRAY_SIZE(logBack.size());
        for (size_t entryNr = 0; entryNr < logBack.size(); entryNr++) {
            auto securityEvent = logBack[entryNr];
            if (securityEvent.containsKey("atnr")) {
                capacity += JSON_ARRAY_SIZE(3);
            } else {
                capacity += JSON_ARRAY_SIZE(2);
            }
        }
    }

    JsonDoc nextLogBack = initJsonDoc(getMemoryTag(), capacity);

    if (logBackLoaded) {
        // Copy old Json object to new
        nextLogBack = logBack;
    }

    // Append SecurityEvent
    auto securityEvent = nextLogBack.createNestedObject();
    securityEvent["type"] = eventType;

    Timestamp time = context.getClock().now();
    char timeStr [MO_JSONDATE_SIZE] = {'\0'};
    if (!context.getClock().toJsonString(time, timeStr, sizeof(timeStr))) {
        MO_DBG_ERR("Serialization error");
        return false;
    }
    securityEvent["time"] = (const char*)timeStr; // force zero-copy mode

    securityEvent["atnr"] = 0;

    if (nextLogBack.overflowed()) {
        MO_DBG_ERR("Serialization error");
        return false;
    }

    // Update logfile on flash
    char fn [MO_MAX_PATH_SIZE] = {'\0'};
    auto ret = snprintf(fn, sizeof(fn), MO_SECLOG_FN_PREFIX "%u.jsn", fileNrBack);
    if (ret < 0 || (size_t)ret >= sizeof(fn)) {
        MO_DBG_ERR("fn error: %i", ret);
        return false;
    }

    if (FilesystemUtils::storeJson(filesystem, fn, nextLogBack) != FilesystemUtils::StoreStatus::Success) {
        MO_DBG_ERR("FS error: %s", fn);
        return false;
    }

    // Successfully updated end of queue on flash. Update fileNrEnd (remains unchanged when SecurityEvent was appended to existing file)
    fileNrEnd = (fileNrBack + 1) % MO_SECLOG_INDEX_MAX;
    
    // If current front logfile was updated, then need to update cached SecurityEvent counter
    if (fileNrBack == fileNrFront) {
        entryNrEnd++;
    }

    return true;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT
