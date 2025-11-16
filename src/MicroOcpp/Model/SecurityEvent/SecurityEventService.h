// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_SECURITYEVENTSERVICE_H
#define MO_SECURITYEVENTSERVICE_H

#include <MicroOcpp/Model/SecurityEvent/SecurityEvent.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Version.h>

#define MO_SECLOG_FN_PREFIX "slog-"
#define MO_SECLOG_INDEX_MAX 10000
#define MO_SECLOG_INDEX_DIGITS 4 //digits needed to print MAX_INDEX-1 (=9999, i.e. 4 digits)

#ifndef MO_SECLOG_MAX_FILES
#define MO_SECLOG_MAX_FILES 10 // Number of locally stored event log files. Must be smaller than (MO_SECLOG_INDEX_MAX / 2)
#endif

#if !(MO_SECLOG_MAX_FILES < MO_SECLOG_INDEX_MAX / 2)
#error MO_SECLOG_MAX_FILES must be smaller than (MO_SECLOG_INDEX_MAX / 2) to allow ring index initialization from file list
#endif

#ifndef MO_SECLOG_MAX_FILE_ENTRIES
#define MO_SECLOG_MAX_FILE_ENTRIES 20 // Number of SecurityEvents per File
#endif

#ifndef MO_SECLOG_SEND_ATTEMPTS
#define MO_SECLOG_SEND_ATTEMPTS 3 // Number of attempts to send a SecurityEventNotification
#endif

#ifndef MO_SECLOG_SEND_ATTEMPTS
#define MO_SECLOG_SEND_ATTEMPTS 3 // Number of attempts to send a SecurityEventNotification
#endif

#ifndef MO_SECLOG_SEND_ATTEMPT_INTERVAL
#define MO_SECLOG_SEND_ATTEMPT_INTERVAL 180
#endif

// Length of field `type` of a SecurityEventNotificationRequest (1.49.1)
#define MO_SECURITY_EVENT_TYPE_LENGTH 50

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT

namespace MicroOcpp {

class Context;

#if MO_ENABLE_V16
namespace v16 {
class Configuration;
}
#endif
#if MO_ENABLE_V201
namespace v201 {
class Variable;
}
#endif

class SecurityEventService : public MemoryManaged {
private:
    Context& context;
    MO_Connection *connection = nullptr;
    MO_FilesystemAdapter *filesystem = nullptr;

    int ocppVersion = -1;

    #if MO_ENABLE_V16
    v16::Configuration *timeAdjustmentReportingThresholdIntV16 = nullptr;
    #endif
    #if MO_ENABLE_V201
    v201::Variable *timeAdjustmentReportingThresholdIntV201 = nullptr;
    #endif

    Timestamp trackUptime;
    Timestamp trackUnixTime;
    int32_t trackTimeOffset = 0;

    unsigned int fileNrBegin = 0; //oldest (historical) log file on flash. Already sent via SecurityEventNotification, can still be fetched via GetLog
    unsigned int fileNrFront = 0; //oldest log which is still queued to be sent to the server
    unsigned int fileNrEnd = 0; //one position behind newest log file

    // Front logfile entries. entryNr specifies the position of a SecurityEvent in the front logfile
    unsigned int entryNrFront = 0; //same logic as fileNr. The "Begin" number is always 0 and is not needed explict here
    unsigned int entryNrEnd = 0;

    Timestamp attemptTime;
    unsigned int attemptNr = 0;
    bool isSecurityEventInProgress = false;

    bool fetchSecurityEventFront(char *type, Timestamp& timestamp, unsigned int& attemptNr); //read current front event into output parameters
    bool advanceSecurityEventFront();

public:
    SecurityEventService(Context& context);

    bool setup();

    void loop();

    bool triggerSecurityEvent(const char *eventType);
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT
#endif
