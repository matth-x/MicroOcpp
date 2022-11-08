// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef SMARTCHARGINGSERVICE_H
#define SMARTCHARGINGSERVICE_H

#define CHARGEPROFILEMAXSTACKLEVEL 8
#define CHARGINGSCHEDULEMAXPERIODS 24
#define MAXCHARGINGPROFILESINSTALLED 10

#include <ArduinoJson.h>
#include <functional>

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

using OnLimitChange = std::function<void(float)>;

class OcppEngine;

class SmartChargingService {
private:
    OcppEngine& context;
    
    const float DEFAULT_CHARGE_LIMIT;
    const float V_eff; //use for approximation: chargingLimit in A * V_eff = chargingLimit in W
    ChargingProfile *ChargePointMaxProfile[CHARGEPROFILEMAXSTACKLEVEL];
    ChargingProfile *TxDefaultProfile[CHARGEPROFILEMAXSTACKLEVEL];
    ChargingProfile *TxProfile[CHARGEPROFILEMAXSTACKLEVEL];
    OnLimitChange onLimitChange = NULL;
    float limitBeforeChange;
    OcppTimestamp nextChange;

    bool chargingSessionStateInitialized {false};
    std::shared_ptr<Configuration<const char*>> txStartTime;
    OcppTimestamp chargingSessionStart;
    int chargingSessionTransactionID;
    std::shared_ptr<Configuration<int>> sRmtProfileId;
    uint16_t sRmtProfileIdRev {0};
    uint16_t sessionIdTagRev {0};
    void refreshChargingSessionState();

    ChargingProfile *updateProfileStack(JsonObject json);
    FilesystemOpt filesystemOpt;
    bool writeProfileToFlash(JsonObject json, ChargingProfile *chargingProfile);
    bool loadProfiles();
  
public:
    SmartChargingService(OcppEngine& context, float chargeLimit, float V_eff, int numConnectors, FilesystemOpt filesystemOpt = FilesystemOpt::Use_Mount_FormatOnFail);
    void setChargingProfile(JsonObject json);
    bool clearChargingProfile(const std::function<bool(int, int, ChargingProfilePurposeType, int)>& filter);
    void inferenceLimit(const OcppTimestamp &t, float *limit, OcppTimestamp *validTo);
    float inferenceLimitNow();
    void setOnLimitChange(OnLimitChange onLimitChange);
    ChargingSchedule *getCompositeSchedule(int connectorId, otime_t duration);
    void loop();
};

} //end namespace ArduinoOcpp
#endif
