// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SMARTCHARGINGMODEL_H
#define MO_SMARTCHARGINGMODEL_H

#ifndef MO_ChargeProfileMaxStackLevel
#define MO_ChargeProfileMaxStackLevel 8
#endif

#ifndef MO_ChargeProfileMaxStackLevel_digits
#define MO_ChargeProfileMaxStackLevel_digits 1 //digits needed to print MO_ChargeProfileMaxStackLevel (=8, i.e. 1 digit)
#endif

#if MO_ChargeProfileMaxStackLevel >= 10 && MO_ChargeProfileMaxStackLevel_digits < 2
#error Need to set build flag MO_ChargeProfileMaxStackLevel_digits
#endif

#ifndef MO_ChargingScheduleMaxPeriods
#define MO_ChargingScheduleMaxPeriods 24
#endif

#ifndef MO_MaxChargingProfilesInstalled
#define MO_MaxChargingProfilesInstalled 10
#endif

#include <memory>
#include <limits>

#include <ArduinoJson.h>

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float power; //in W. Negative if undefined
    float current; //in A. Negative if undefined
    int numberPhases; //Negative if undefined

    #if MO_ENABLE_V201
    int phaseToUse; //select between L1, L2, L3 (negative if undefined)
    #endif //MO_ENABLE_V201
} MO_ChargeRate;

void mo_chargeRate_init(MO_ChargeRate *limit); //initializes all fields to -1
bool mo_chargeRate_equals(const MO_ChargeRate *a, const MO_ChargeRate *b);

#ifdef __cplusplus
} //extern "C"

namespace MicroOcpp {

class Clock;

enum class ChargingProfilePurposeType : uint8_t {
    UNDEFINED,
    ChargePointMaxProfile,
    TxDefaultProfile,
    TxProfile
};
const char *serializeChargingProfilePurposeType(ChargingProfilePurposeType v);
ChargingProfilePurposeType deserializeChargingProfilePurposeType(const char *str);

enum class ChargingProfileKindType : uint8_t {
    Absolute,
    Recurring,
    Relative
};

enum class RecurrencyKindType : uint8_t {
    UNDEFINED, //MO-internal
    Daily,
    Weekly
};

enum class ChargingRateUnitType : uint8_t {
    UNDEFINED, //MO-internal
    Watt,
    Amp
};
const char *serializeChargingRateUnitType(ChargingRateUnitType v);
ChargingRateUnitType deserializeChargingRateUnitType(const char *str);

class ChargingSchedulePeriod {
public:
    int startPeriod;
    float limit;
    int numberPhases = 3;
    #if MO_ENABLE_V201
    int phaseToUse = -1;
    #endif
};

class ChargingSchedule : public MemoryManaged {
public:
    #if MO_ENABLE_V201
    int id = -1; //defined for ChargingScheduleType (2.12), not used for CompositeScheduleType (2.18)
    #endif
    int duration = -1;
    int32_t startSchedule = 0; //unix time. 0 if undefined
    ChargingRateUnitType chargingRateUnit;
    ChargingSchedulePeriod **chargingSchedulePeriod = nullptr;
    size_t chargingSchedulePeriodSize = 0;
    float minChargingRate = -1.0f;

    ChargingProfileKindType chargingProfileKind; //copied from ChargingProfile to increase cohesion of limit algorithms
    RecurrencyKindType recurrencyKind = RecurrencyKindType::UNDEFINED; //copied from ChargingProfile to increase cohesion of limit algorithms

    ChargingSchedule();

    ~ChargingSchedule();

    bool resizeChargingSchedulePeriod(size_t chargingSchedulePeriodSize);

    /**
     * limit: output parameter
     * nextChange: output parameter
     * 
     * returns if charging profile defines a limit at time t
     *       if true, limit and nextChange will be set according to this Schedule
     *       if false, only nextChange will be set
     */
    bool calculateLimit(int32_t unixTime, int32_t sessionDurationSecs, MO_ChargeRate& limit, int32_t& nextChangeSecs);

    bool parseJson(Clock& clock, int ocppVersion, JsonObject json);
    size_t getJsonCapacity(int ocppVersion, bool compositeSchedule);
    bool toJson(Clock& clock, int ocppVersion, bool compositeSchedule, JsonObject out, int evseId = -1);

    /*
    * print on console
    */
    void printSchedule(Clock& clock);
};

class ChargingProfile : public MemoryManaged {
public:
    int chargingProfileId = -1;
    #if MO_ENABLE_V16
    int transactionId16 = -1;
    #endif
    #if MO_ENABLE_V201
    char transactionId201 [MO_TXID_SIZE] = {'\0'};
    #endif
    int stackLevel = 0;
    ChargingProfilePurposeType chargingProfilePurpose {ChargingProfilePurposeType::TxProfile};
    ChargingProfileKindType chargingProfileKind {ChargingProfileKindType::Relative}; //copied to ChargingSchedule to increase cohesion of limit algorithms
    RecurrencyKindType recurrencyKind {RecurrencyKindType::UNDEFINED}; // copied to ChargingSchedule to increase cohesion
    int32_t validFrom = 0; //unix time. 0 if undefined
    int32_t validTo = 0; //unix time. 0 if undefined
    ChargingSchedule chargingSchedule;

    ChargingProfile();

    /**
     * limit: output parameter
     * nextChange: output parameter
     * 
     * returns if charging profile defines a limit at time t
     *       if true, limit and nextChange will be set according to this Schedule
     *       if false, only nextChange will be set
     */
    bool calculateLimit(int32_t unixTime, int32_t sessionDurationSecs, MO_ChargeRate& limit, int32_t& nextChangeSecs);

    bool parseJson(Clock& clock, int ocppVersion, JsonObject json);
    size_t getJsonCapacity(int ocppVersion);
    bool toJson(Clock& clock, int ocppVersion, JsonObject out);

    /*
    * print on console
    */
    void printProfile(Clock& clock);
};

} //namespace MicroOcpp
#endif //__cplusplus
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
#endif
