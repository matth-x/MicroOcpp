// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SMARTCHARGINGMODEL_H
#define SMARTCHARGINGMODEL_H

#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

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

class ChargingSchedulePeriod {
private:
    int startPeriod;
    float limit; //one fractural digit at most
    int numberPhases = -1;
public:
    ChargingSchedulePeriod(JsonObject *json);
    ChargingSchedulePeriod(int startPeriod, float limit);
    int getStartPeriod();
    float getLimit();
    void scale(float factor);
    void add(float value);
    int getNumberPhases();
    void printPeriod();
};

class ChargingSchedule {
private:
    int duration = -1;
    OcppTimestamp startSchedule;
    char schedulingUnit; //either 'A' or 'W'
    std::vector<ChargingSchedulePeriod*> chargingSchedulePeriod;
    float minChargingRate = -1.0f;

    ChargingProfileKindType chargingProfileKind; //copied from ChargingProfile to increase cohesion of limit inferencing methods
    RecurrencyKindType recurrencyKind; //copied from ChargingProfile to increase cohesion of limit inferencing methods
public:
    ChargingSchedule(JsonObject *json, ChargingProfileKindType chargingProfileKind, RecurrencyKindType recurrencyKind);
    ChargingSchedule(ChargingSchedule &other);
    ChargingSchedule(const OcppTimestamp &startSchedule, int duration);
    ~ChargingSchedule();

    /**
     * limit: output parameter
     * nextChange: output parameter
     * 
     * returns if charging profile defines a limit at time t
     *       if true, limit and nextChange will be set according to this Schedule
     *       if false, only nextChange will be set
     */
    bool inferenceLimit(const OcppTimestamp &t, const OcppTimestamp &startOfCharging, float *limit, OcppTimestamp *nextChange);

    bool addChargingSchedulePeriod(ChargingSchedulePeriod *period);

    void scale(float factor);
    void translate(float offset);

    DynamicJsonDocument *toJsonDocument();

    /*
    * print on console
    */
    void printSchedule();
};

class ChargingProfile {
private:
    int chargingProfileId = -1;
    int transactionId = -1;
    int stackLevel = 0;
    ChargingProfilePurposeType chargingProfilePurpose;
    ChargingProfileKindType chargingProfileKind; //copied to ChargingSchedule to increase cohesion of limit inferencing methods
    RecurrencyKindType recurrencyKind; // copied to ChargingSchedule to increase cohesion
    OcppTimestamp validFrom;
    OcppTimestamp validTo;
    ChargingSchedule *chargingSchedule;
public:
    ChargingProfile(JsonObject *json);
    ChargingProfile(ChargingSchedule *schedule, int chargingProfileId); //For Energy management. Can be extended with more parameters later
    ~ChargingProfile();

    /**
     * limit: output parameter
     * nextChange: output parameter
     * 
     * returns if charging profile defines a limit at time t
     *       if true, limit and nextChange will be set according to this Schedule
     *       if false, only nextChange will be set
     */
    bool inferenceLimit(const OcppTimestamp &t, const OcppTimestamp &startOfCharging, float *limit, OcppTimestamp *nextChange);

    /*
    * Simpler function if startOfCharging is not available. Caution: This likely will differ from inference with startOfCharging
    */
    bool inferenceLimit(const OcppTimestamp &t, float *limit, OcppTimestamp *nextChange);

    /*
    * Check if this profile belongs to transaction with ID transId
    */
    bool checkTransactionId(int transId);

    int getStackLevel();
    
    ChargingProfilePurposeType getChargingProfilePurpose();

    int getChargingProfileId();

    /*
    * print on console
    */
    void printProfile();
};

} //end namespace ArduinoOcpp
#endif
