// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "Variants.h"

#include "SmartChargingService.h"
#include "OcppEngine.h"

#define SINGLE_CONNECTOR_ID 1

SmartChargingService::SmartChargingService(float chargeLimit, int numConnectors)
      : DEFAULT_CHARGE_LIMIT(chargeLimit) {
  
  if (numConnectors >= 2) {
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

void SmartChargingService::updateChargingProfile(JsonObject *json){
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
    case (ChargingProfilePurposeType::ChargePointMaxProfile):
      profilePurposeStack = ChargePointMaxProfile;
      break;
    case (ChargingProfilePurposeType::TxDefaultProfile):
      profilePurposeStack = TxDefaultProfile;
      break;
    case (ChargingProfilePurposeType::TxProfile):
      profilePurposeStack = TxProfile;
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
}
