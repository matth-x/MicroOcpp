// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include "Variants.h"

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

#include <ArduinoOcpp/Core/Configuration.h>

#if defined(ESP32) && !defined(AO_DEACTIVATE_FLASH)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif

#define SINGLE_CONNECTOR_ID 1

#define PROFILE_FN_PREFIX "/ocpp-"
#define PROFILE_FN_SUFFIX ".cnf"
#define PROFILE_CUSTOM_CAPACITY 500
#define PROFILE_MAX_CAPACITY 4000

using namespace::ArduinoOcpp;

SmartChargingService::SmartChargingService(float chargeLimit, int numConnectors, OcppTime *ocppTime, FilesystemOpt filesystemOpt)
      : DEFAULT_CHARGE_LIMIT(chargeLimit), ocppTime(ocppTime), filesystemOpt(filesystemOpt) {
  
  if (numConnectors > 2) {
    Serial.print(F("[SmartChargingService] Error: Unfortunately, multiple connectors are not implemented in SmartChargingService yet. Only connector 1 will receive charging limits\n"));
  }
  
  limitBeforeChange = -1.0f;
  nextChange = INFINITY_THLD;
  chargingSessionStart = INFINITY_THLD;
  chargingSessionTransactionID = -1;
  chargingSessionIsActive = false;
  for (int i = 0; i < CHARGEPROFILEMAXSTACKLEVEL; i++) {
    ChargePointMaxProfile[i] = NULL;
    TxDefaultProfile[i] = NULL;
    TxProfile[i] = NULL;
  }
  setSmartChargingService(this); //in OcppEngine.cpp
  declareConfiguration("ChargeProfileMaxStackLevel", CHARGEPROFILEMAXSTACKLEVEL, CONFIGURATION_FN, false, true, false, true);

  loadProfiles();
}

void SmartChargingService::loop(){
  /**
   * check if to call onLimitChange
   */
  if (now() >= nextChange){
    float limit = -1.0f;
    time_t validTo = 0;
    inferenceLimit(now(), &limit, &validTo);
    if (DEBUG_OUT) Serial.print(F("[SmartChargingService] New Limit! Values: {scheduled at = "));
    if (DEBUG_OUT) printTime(nextChange);
    if (DEBUG_OUT) Serial.print(F(", nextChange = "));
    if (DEBUG_OUT) printTime(validTo);
    if (DEBUG_OUT) Serial.print(F(", limit = "));
    if (DEBUG_OUT) Serial.print(limit);
    if (DEBUG_OUT) Serial.print(F("}\n"));
    nextChange = validTo;
    if (limit != limitBeforeChange){
      if (onLimitChange != NULL) {
        onLimitChange(limit);
      }
    }
    limitBeforeChange = limit;

    //ChargePointStatusService:
    ChargePointStatusService *cpStatusService = getChargePointStatusService();    
    if (cpStatusService != NULL) {
      if (limit > 0.0f) {
        //cpStatusService->getConnector(SINGLE_CONNECTOR_ID)->startEnergyOffer();  //No, the client implementation should decide what to do here! TODO review if needed
        //cpStatusService->startEnergyOffer();
      } else {
        //cpStatusService->getConnector(SINGLE_CONNECTOR_ID)->stopEnergyOffer();   //No, the client implementation should decide what to do here! TODO review if needed
        //cpStatusService->stopEnergyOffer();
      }
    }
  }
}

float SmartChargingService::inferenceLimitNow(){
  float limit = 0.0f;
  time_t validTo; //not needed
  inferenceLimit(now(), &limit, &validTo);
  return limit;
}

void SmartChargingService::setOnLimitChange(OnLimitChange onLtChg){
  onLimitChange = onLtChg;
}

/**
 * validToOutParam: The begin of the next SmartCharging restriction after time t. It is not taken into
 * account if the next Profile will be a prevailing one. If the profile at time t ends before any
 * other profile engages, the end of this profile will be written into validToOutParam.
 */
void SmartChargingService::inferenceLimit(time_t t, float *limitOutParam, time_t *validToOutParam){
  time_t validToMin = INFINITY_THLD;
  /*
   * TxProfile rules over TxDefaultProfile. ChargePointMaxProfile rules over both of them
   * 
   * if (TxProfile is present)
   *       take limit from TxProfile with the highest stackLevel
   * else
   *       take limit from TxDefaultProfile with the highest stackLevel
   * take maximum from ChargePointMaxProfile with the highest stackLevel
   * return minimum(limit, maximum from ChargePointMaxProfile)
   */

  //evaluate limit from TxProfiles
  float limit_tx = 0.0f;
  bool limit_defined_tx = false;
  for (int i = CHARGEPROFILEMAXSTACKLEVEL - 1; i >= 0; i--){
    if (TxProfile[i] == NULL) continue;
    if (!TxProfile[i]->checkTransactionId(chargingSessionTransactionID)) continue;
    time_t nextChange = INFINITY_THLD;
    limit_defined_tx = TxProfile[i]->inferenceLimit(t, chargingSessionStart, &limit_tx, &nextChange);
    validToMin = minimum(validToMin, nextChange); //nextChange is always >= t here
    if (limit_defined_tx) {
      //The valid profile with the highest stack level is found. It prevails over any other so end this loop.
      break;
    }
  }

  //evaluate limit from TxDefaultProfiles
  float limit_txdef = 0.0f;
  bool limit_defined_txdef = false;
  for (int i = CHARGEPROFILEMAXSTACKLEVEL - 1; i >= 0; i--){
    if (TxDefaultProfile[i] == NULL) continue;
    if (!TxDefaultProfile[i]->checkTransactionId(chargingSessionTransactionID)) continue; //this doesn't do anything on TxDefaultProfiles and could be deleted
    time_t nextChange = INFINITY_THLD;
    limit_defined_txdef = TxDefaultProfile[i]->inferenceLimit(t, chargingSessionStart, &limit_txdef, &nextChange);
    validToMin = minimum(validToMin, nextChange); //nextChange is always >= t here
    if (limit_defined_txdef) {
      //The valid profile with the highest stack level is found. It prevails over any other so end this loop.
      break;
    }
  }

  //evaluate limit from ChargePointMaxProfile
  float limit_cpmax = 0.0f;
  bool limit_defined_cpmax = false;
  for (int i = CHARGEPROFILEMAXSTACKLEVEL - 1; i >= 0; i--){
    if (ChargePointMaxProfile[i] == NULL) continue;
    if (!ChargePointMaxProfile[i]->checkTransactionId(chargingSessionTransactionID)) continue; //this doesn't do anything on ChargePointMaxProfiles and could be deleted
    time_t nextChange = INFINITY_THLD;
    limit_defined_cpmax = ChargePointMaxProfile[i]->inferenceLimit(t, chargingSessionStart, &limit_cpmax, &nextChange);
    validToMin = minimum(validToMin, nextChange);
    if (limit_defined_cpmax) {
      //The valid profile with the highest stack level is found. It prevails over any other so end this loop.
      break;
    }
  }

  *validToOutParam = validToMin; //validTo output parameter has successfully been determined here

  //choose which limit to set according to specification
  bool applicable_profile_found = false;
  if (limit_defined_txdef){
    *limitOutParam = limit_txdef;
    applicable_profile_found = true;
  }
  if (limit_defined_tx){
    *limitOutParam = limit_tx;
    applicable_profile_found = true;
  }
  if (limit_defined_cpmax){
    //Warning: This block MUST be rewritten when multiple connector support is introduced
    if (applicable_profile_found) {
      if (limit_cpmax < *limitOutParam){
        //TxProfile or TxDefaultProfile exceeds the maximum for the whole CP 
        *limitOutParam = limit_cpmax;
      } //else: TxProfile or TxDefaultProfile are within their boundary. Do nothing
    } else {
      //No TxProfile or TxDefaultProfile found. The limit is set to the maximum of the CP
      *limitOutParam = limit_cpmax;
    }
    applicable_profile_found = true;
  }
  if (!applicable_profile_found) {
    *limitOutParam = DEFAULT_CHARGE_LIMIT;
  }
}

void SmartChargingService::writeOutCompositeSchedule(JsonObject *json){
  Serial.print(F("[SmartChargingService] Unsupported Operation: SmartChargingService::writeOutCompositeSchedule\n"));
}

void SmartChargingService::beginCharging(time_t t, int transactionID){
  if (chargingSessionIsActive) {
    Serial.print(F("[SmartChargingService] Error: begin new Charging session though there is already running one!\n"));
  }
  chargingSessionStart = t;
  chargingSessionTransactionID = transactionID;
  chargingSessionIsActive = true;
  nextChange = minimum(t, nextChange);
}

void SmartChargingService::beginChargingNow(){
  beginCharging(now(), -1);
}

void SmartChargingService::endChargingNow(){
  if (!chargingSessionIsActive) {
    Serial.print(F("[SmartChargingService] Error: end Charging session but there isn't running one!\n"));
  }
  
  chargingSessionStart = INFINITY_THLD;
  chargingSessionTransactionID = -1;
  chargingSessionIsActive = false;
  nextChange = now();
}

void SmartChargingService::updateChargingProfile(JsonObject *json) {
  ChargingProfile *pointer = updateProfileStack(json);
  if (pointer)
    writeProfileToFlash(json, pointer);
}

ChargingProfile *SmartChargingService::updateProfileStack(JsonObject *json){
  ChargingProfile *chargingProfile = new ChargingProfile(json);

  if (DEBUG_OUT) {
    Serial.print(F("[SmartChargingService] Charging Profile internal model\n"));
    chargingProfile->printProfile();
  }

  int stackLevel = chargingProfile->getStackLevel();
  if (stackLevel >= CHARGEPROFILEMAXSTACKLEVEL) {
    Serial.print(F("[SmartChargingService] Error: Stacklevel of Charging Profile is greater than CHARGEPROFILEMAXSTACKLEVEL\n"));
    stackLevel = CHARGEPROFILEMAXSTACKLEVEL - 1;
  }

  ChargingProfile **profilePurposeStack; //select which stack this profile belongs to due to its purpose

  switch (chargingProfile->getChargingProfilePurpose()) {
    case (ChargingProfilePurposeType::TxDefaultProfile):
      profilePurposeStack = TxDefaultProfile;
      break;
    case (ChargingProfilePurposeType::TxProfile):
      profilePurposeStack = TxProfile;
      break;
    default:
    //case (ChargingProfilePurposeType::ChargePointMaxProfile):
      profilePurposeStack = ChargePointMaxProfile;
      break;
  }
  
  if (profilePurposeStack[stackLevel] != NULL){
    delete profilePurposeStack[stackLevel];
  }

  profilePurposeStack[stackLevel] = chargingProfile;

  /**
   * Invalidate the last limit inference by setting the nextChange to now. By the next loop()-call, the limit
   * and nextChange will be recalculated and onLimitChanged will be called.
   */
  nextChange = now();

  return chargingProfile;
}

bool SmartChargingService::writeProfileToFlash(JsonObject *json, ChargingProfile *chargingProfile) {
#ifndef AO_DEACTIVATE_FLASH

  if (!filesystemOpt.accessAllowed()) {
    if (DEBUG_OUT) Serial.println(F("[SmartChargingService] Prohibit access to FS"));
    return true;
  }
  
  String profileFN = PROFILE_FN_PREFIX;

  switch (chargingProfile->getChargingProfilePurpose()) {
    case (ChargingProfilePurposeType::ChargePointMaxProfile):
      profileFN += "ChargePointMaxProfile-";
      break;
    case (ChargingProfilePurposeType::TxDefaultProfile):
      profileFN += "TxDefaultProfile-";
      break;
    case (ChargingProfilePurposeType::TxProfile):
      profileFN += "TxProfile-";
      break;
  }

  profileFN += chargingProfile->getStackLevel();
  profileFN += PROFILE_FN_SUFFIX;

  USE_FS.remove(profileFN);

  File file = USE_FS.open(profileFN, "w");

  if (!file) {
      Serial.print(F("[SmartChargingService] Unable to save: could not save profile: "));
      Serial.println(profileFN);
      return false;
  }

  // Serialize JSON to file
  if (serializeJson(*json, file) == 0) {
      Serial.println(F("[SmartChargingService] Unable to save: Could not serialize JSON for profile: "));
      Serial.println(profileFN);
      file.close();
      return false;
  }

  //success
  file.close();
  if (DEBUG_OUT) Serial.print(F("[SmartChargingService] Saving profile successful\n"));

  // BEGIN DEBUG
  if (DEBUG_OUT) {
    file = USE_FS.open(profileFN, "r");

    Serial.println(file.readStringUntil('\n'));

    file.close();
    // END DEBUG
  }

#endif //ndef AO_DEACTIVATE_FLASH
  return true;
}

bool SmartChargingService::loadProfiles() {
#ifndef AO_DEACTIVATE_FLASH
    if (!filesystemOpt.accessAllowed()) {
        if (DEBUG_OUT) Serial.println(F("[SmartChargingService] Prohibit access to FS"));
        return true;
    }
    
    const int N_PURPOSES = 3;
    ChargingProfilePurposeType purposes[N_PURPOSES] = {ChargingProfilePurposeType::ChargePointMaxProfile, ChargingProfilePurposeType::TxDefaultProfile, ChargingProfilePurposeType::TxProfile};

    for (int iStack = 0; iStack < N_PURPOSES; iStack++) {
      ChargingProfile **profilePurposeStack; //select which stack this profile belongs to due to its purpose
      String profileFnPurpose;

      switch (purposes[iStack]) {
        case (ChargingProfilePurposeType::ChargePointMaxProfile):
          profilePurposeStack = ChargePointMaxProfile;
          profileFnPurpose = "ChargePointMaxProfile-";
          break;
        case (ChargingProfilePurposeType::TxDefaultProfile):
          profilePurposeStack = TxDefaultProfile;
          profileFnPurpose = "TxDefaultProfile-";
          break;
        case (ChargingProfilePurposeType::TxProfile):
          profilePurposeStack = TxProfile;
          profileFnPurpose = "TxProfile-";
          break;
      }

      for (int iLevel = 0; iLevel < CHARGEPROFILEMAXSTACKLEVEL; iLevel++) {

        String profileFN = PROFILE_FN_PREFIX;
        profileFN += profileFnPurpose;
        profileFN += iLevel;
        profileFN += PROFILE_FN_SUFFIX;

        File file = USE_FS.open(profileFN, "r");

        if (file) {
            if (DEBUG_OUT) Serial.print(F("[SmartChargingService] Load profile from file: "));
            if (DEBUG_OUT) Serial.println(profileFN);
        } else {
            continue; //There is not a profile on the stack iStack with stacklevel iLevel. Normal case, just continue.
        }

        if (!file.available()) {
            Serial.print(F("[SmartChargingService] Unable to initialize: empty file for profile: "));
            Serial.println(profileFN);
            file.close();
            return false;
        }

        int file_size = file.size();

        if (file_size < 2) {
            Serial.print(F("[SmartChargingService] Unable to initialize: too short for json: "));
            Serial.println(profileFN);
            continue;
        }
        
        size_t capacity = 2*file_size;
        if (capacity < PROFILE_CUSTOM_CAPACITY)
          capacity = PROFILE_CUSTOM_CAPACITY;
        if (capacity > PROFILE_MAX_CAPACITY)
          capacity = PROFILE_MAX_CAPACITY;

        while (capacity <= PROFILE_MAX_CAPACITY) {
          bool increaseCapacity = false;
          bool error = true;

          DynamicJsonDocument profileDoc(capacity);

          DeserializationError jsonError = deserializeJson(profileDoc, file);
          switch (jsonError.code()) {
            case DeserializationError::Ok:
                error = false;
                break;
            case DeserializationError::InvalidInput:
                Serial.print(F("[SmartChargingService] Unable to initialize: Invalid json in file: "));
                Serial.println(profileFN);
                break;
            case DeserializationError::NoMemory:
                increaseCapacity = true;
                error = false;
                break;
            default:
                Serial.print(F("[SmartChargingService] Unable to initialize: Error in file: "));
                Serial.println(profileFN);
                break;
          }

          if (error) {
              break;
          }

          if (increaseCapacity) {
            capacity *= 1.5;
            file.seek(0, SeekSet); //rewind file to beginning
            if (DEBUG_OUT) Serial.print(F("[SmartChargingService] Initialization: increase JsonCapacity to "));
            if (DEBUG_OUT) Serial.print(capacity, DEC);
            if (DEBUG_OUT) Serial.print(F("for file: "));
            if (DEBUG_OUT) Serial.println(profileFN);
            continue;
          }

          JsonObject profileJson = profileDoc.as<JsonObject>();
          updateProfileStack(&profileJson);

          profileDoc.clear();
          break;
        }

        file.close();
      }
    }

#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}
