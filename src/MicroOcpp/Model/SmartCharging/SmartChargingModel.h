// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SMARTCHARGINGMODEL_H
#define SMARTCHARGINGMODEL_H

#define CHARGEPROFILEMAXSTACKLEVEL 8
#define CHARGINGSCHEDULEMAXPERIODS 24
#define MAXCHARGINGPROFILESINSTALLED 10

#include <memory>
#include <vector>
#include <limits>

#include <ArduinoJson.h>

#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

enum class ChargingProfilePurposeType {
    ChargePointMaxProfile,
    TxDefaultProfile,
    TxProfile
};

enum class ChargingProfileKindType {
    Absolute,
    Recurring,
    Relative
};

enum class RecurrencyKindType {
    NOT_SET, //not part of OCPP 1.6
    Daily,
    Weekly
};

enum class ChargingRateUnitType {
    Watt,
    Amp
};

struct ChargeRate {
    float power = std::numeric_limits<float>::max();
    float current = std::numeric_limits<float>::max();
    int nphases = std::numeric_limits<int>::max();

    bool operator==(const ChargeRate& rhs) {
        return power == rhs.power &&
               current == rhs.current &&
               nphases == rhs.nphases;
    }
    bool operator!=(const ChargeRate& rhs) {
        return !(*this == rhs);
    }
};

//returns a new vector with the minimum of each component
ChargeRate chargeRate_min(const ChargeRate& a, const ChargeRate& b);

class ChargingSchedulePeriod {
public:
    int startPeriod;
    float limit;
    int numberPhases = 3;
};

class ChargingSchedule {
public:
    int duration = -1;
    Timestamp startSchedule;
    ChargingRateUnitType chargingRateUnit;
    std::vector<ChargingSchedulePeriod> chargingSchedulePeriod;
    float minChargingRate = -1.0f;

    ChargingProfileKindType chargingProfileKind; //copied from ChargingProfile to increase cohesion of limit algorithms
    RecurrencyKindType recurrencyKind = RecurrencyKindType::NOT_SET; //copied from ChargingProfile to increase cohesion of limit algorithms

    /**
     * limit: output parameter
     * nextChange: output parameter
     * 
     * returns if charging profile defines a limit at time t
     *       if true, limit and nextChange will be set according to this Schedule
     *       if false, only nextChange will be set
     */
    bool calculateLimit(const Timestamp &t, const Timestamp &startOfCharging, ChargeRate& limit, Timestamp& nextChange);

    bool toJson(DynamicJsonDocument& out);

    /*
    * print on console
    */
    void printSchedule();
};

class ChargingProfile {
public:
    int chargingProfileId = -1;
    int transactionId = -1;
    int stackLevel = 0;
    ChargingProfilePurposeType chargingProfilePurpose {ChargingProfilePurposeType::TxProfile};
    ChargingProfileKindType chargingProfileKind {ChargingProfileKindType::Relative}; //copied to ChargingSchedule to increase cohesion of limit algorithms
    RecurrencyKindType recurrencyKind {RecurrencyKindType::NOT_SET}; // copied to ChargingSchedule to increase cohesion
    Timestamp validFrom;
    Timestamp validTo;
    ChargingSchedule chargingSchedule;

    /**
     * limit: output parameter
     * nextChange: output parameter
     * 
     * returns if charging profile defines a limit at time t
     *       if true, limit and nextChange will be set according to this Schedule
     *       if false, only nextChange will be set
     */
    bool calculateLimit(const Timestamp &t, const Timestamp &startOfCharging, ChargeRate& limit, Timestamp& nextChange);

    /*
    * Simpler function if startOfCharging is not available. Caution: This likely will differ from function with startOfCharging
    */
    bool calculateLimit(const Timestamp &t, ChargeRate& limit, Timestamp& nextChange);

    int getChargingProfileId();
    int getTransactionId();
    int getStackLevel();
    
    ChargingProfilePurposeType getChargingProfilePurpose();

    bool toJson(DynamicJsonDocument& out);

    /*
    * print on console
    */
    void printProfile();
};

std::unique_ptr<ChargingProfile> loadChargingProfile(JsonObject& json);

bool loadChargingSchedule(JsonObject& json, ChargingSchedule& out);

} //end namespace MicroOcpp

#endif
