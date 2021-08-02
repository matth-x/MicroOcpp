// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SMARTCHARGINGSERVICE_H
#define SMARTCHARGINGSERVICE_H

#define CHARGEPROFILEMAXSTACKLEVEL 20

#include <ArduinoJson.h>
#include <functional>

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

//typedef void (*OnLimitChange)(float newLimit);
typedef std::function<void(float)> OnLimitChange;

class SmartChargingService {
private:
    const float DEFAULT_CHARGE_LIMIT;
    const float V_eff; //use for approximation: chargingLimit in A * V_eff = chargingLimit in W
    OcppTime *ocppTime;
    ChargingProfile *ChargePointMaxProfile[CHARGEPROFILEMAXSTACKLEVEL];
    ChargingProfile *TxDefaultProfile[CHARGEPROFILEMAXSTACKLEVEL];
    ChargingProfile *TxProfile[CHARGEPROFILEMAXSTACKLEVEL];
    OnLimitChange onLimitChange = NULL;
    float limitBeforeChange;
    OcppTimestamp nextChange;
    OcppTimestamp chargingSessionStart;
    int chargingSessionTransactionID;
    void refreshChargingSessionState();
    //bool chargingSessionIsActive;

    ChargingProfile *updateProfileStack(JsonObject *json);
    FilesystemOpt filesystemOpt;
    bool writeProfileToFlash(JsonObject *json, ChargingProfile *chargingProfile);
    bool loadProfiles();
  
public:
    SmartChargingService(float chargeLimit, float V_eff, int numConnectors, OcppTime *ocppTime, FilesystemOpt filesystemOpt = FilesystemOpt::Use_Mount_FormatOnFail);
    //void beginCharging(const OcppTimestamp &t, int transactionID);
    //void beginChargingNow();
    //void endChargingNow();
    void updateChargingProfile(JsonObject *json);
    bool clearChargingProfile(std::function<bool(int, int, ChargingProfilePurposeType, int)> filter);
    void inferenceLimit(const OcppTimestamp &t, float *limit, OcppTimestamp *validTo);
    float inferenceLimitNow();
    void setOnLimitChange(OnLimitChange onLimitChange);
    void writeOutCompositeSchedule(JsonObject *json);
    void loop();
};

} //end namespace ArduinoOcpp
#endif
