// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SMARTCHARGINGSERVICE_H
#define SMARTCHARGINGSERVICE_H

#define CHARGEPROFILEMAXSTACKLEVEL 20

#include <ArduinoJson.h>
#include <TimeLib.h>
#include <functional>

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>

namespace ArduinoOcpp {

//typedef void (*OnLimitChange)(float newLimit);
typedef std::function<void(float)> OnLimitChange;

class SmartChargingService {
private:
  const float DEFAULT_CHARGE_LIMIT;
  ChargingProfile *ChargePointMaxProfile[CHARGEPROFILEMAXSTACKLEVEL];
  ChargingProfile *TxDefaultProfile[CHARGEPROFILEMAXSTACKLEVEL];
  ChargingProfile *TxProfile[CHARGEPROFILEMAXSTACKLEVEL];
  OnLimitChange onLimitChange;
  float limitBeforeChange;
  time_t nextChange;
  time_t chargingSessionStart;
  int chargingSessionTransactionID;
  bool chargingSessionIsActive;

  ChargingProfile *updateProfileStack(JsonObject *json);
  FilesystemOpt filesystemOpt;
  bool writeProfileToFlash(JsonObject *json, ChargingProfile *chargingProfile);
  bool loadProfiles();
  
public:
  SmartChargingService(float chargeLimit, int numConnectors, FilesystemOpt filesystemOpt = FilesystemOpt::Use_Mount_FormatOnFail);
  void beginCharging(time_t t, int transactionID);
  void beginChargingNow();
  void endChargingNow();
  void updateChargingProfile(JsonObject *json);
  void inferenceLimit(time_t t, float *limit, time_t *validTo);
  float inferenceLimitNow();
  void setOnLimitChange(OnLimitChange onLimitChange);
  void writeOutCompositeSchedule(JsonObject *json);
  void loop();
};

} //end namespace ArduinoOcpp
#endif
