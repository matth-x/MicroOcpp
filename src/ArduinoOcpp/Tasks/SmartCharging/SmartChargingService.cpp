// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#if !defined(AO_DEACTIVATE_FLASH) && defined(AO_DEACTIVATE_FLASH_SMARTCHARGING)
#define AO_DEACTIVATE_FLASH
#endif

#ifndef AO_DEACTIVATE_FLASH
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#if AO_USE_FILEAPI == ESPIDF_SPIFFS
#define AO_DEACTIVATE_FLASH //migrate to File utils
#endif
#endif

#ifndef AO_DEACTIVATE_FLASH
#if defined(ESP32)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif
#endif

#define SINGLE_CONNECTOR_ID 1

#define PROFILE_FN_PREFIX "/ocpp-"
#define PROFILE_FN_SUFFIX ".cnf"
#define PROFILE_FN_MAXSIZE 30
#define PROFILE_CUSTOM_CAPACITY 500
#define PROFILE_MAX_CAPACITY 4000

using namespace::ArduinoOcpp;

SmartChargingService::SmartChargingService(OcppEngine& context, float chargeLimit, float V_eff, int numConnectors, FilesystemOpt filesystemOpt)
      : context(context), DEFAULT_CHARGE_LIMIT{chargeLimit}, V_eff{V_eff}, filesystemOpt{filesystemOpt} {
  
    if (numConnectors > 2) {
        AO_DBG_ERR("Only one connector supported at the moment");
    }
    
    limitBeforeChange = -1.0f;
    nextChange = MIN_TIME;
    chargingSessionStart = MAX_TIME;
    char max_timestamp [JSONDATE_LENGTH + 1] = {'\0'};
    chargingSessionStart.toJsonString(max_timestamp, JSONDATE_LENGTH + 1);
    txStartTime = declareConfiguration<const char*>("AO_TXSTARTTIME_CONN_1", max_timestamp, CONFIGURATION_FN, false, false, true, false);
    chargingSessionTransactionID = -1;
    sRmtProfileId = declareConfiguration<int>("AO_SRMTPROFILEID_CONN_1", -1, CONFIGURATION_FN, false, false, true, false);
    for (int i = 0; i < CHARGEPROFILEMAXSTACKLEVEL; i++) {
        ChargePointMaxProfile[i] = NULL;
        TxDefaultProfile[i] = NULL;
        TxProfile[i] = NULL;
    }
    declareConfiguration<int>("ChargeProfileMaxStackLevel", CHARGEPROFILEMAXSTACKLEVEL, CONFIGURATION_VOLATILE, false, true, false, false);
    declareConfiguration<const char*>("ChargingScheduleAllowedChargingRateUnit ", "Power", CONFIGURATION_VOLATILE, false, true, false, false);
    declareConfiguration<int>("ChargingScheduleMaxPeriods", CHARGINGSCHEDULEMAXPERIODS, CONFIGURATION_VOLATILE, false, true, false, false);
    declareConfiguration<int>("MaxChargingProfilesInstalled", MAXCHARGINGPROFILESINSTALLED, CONFIGURATION_VOLATILE, false, true, false, false);

    const char *fpId = "SmartCharging";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpId)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpId;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }

    loadProfiles();
}

void SmartChargingService::loop(){

    refreshChargingSessionState();

    /**
     * check if to call onLimitChange
     */
    if (context.getOcppModel().getOcppTime().getOcppTimestampNow() >= nextChange){
        auto& tNow = context.getOcppModel().getOcppTime().getOcppTimestampNow();
        float limit = -1.0f;
        OcppTimestamp validTo = OcppTimestamp();
        inferenceLimit(tNow, &limit, &validTo);

#if (AO_DBG_LEVEL >= AO_DL_INFO)
        char timestamp1[JSONDATE_LENGTH + 1] = {'\0'};
        nextChange.toJsonString(timestamp1, JSONDATE_LENGTH + 1);
        char timestamp2[JSONDATE_LENGTH + 1] = {'\0'};
        validTo.toJsonString(timestamp2, JSONDATE_LENGTH + 1);
        AO_DBG_INFO("New limit for connector 1, scheduled at = %s, nextChange = %s, limit = %f",
                            timestamp1, timestamp2, limit);
#endif

        nextChange = validTo;
        if (limit != limitBeforeChange){
            if (onLimitChange != NULL) {
                onLimitChange(limit);
            }
        }
        limitBeforeChange = limit;
    }
}

float SmartChargingService::inferenceLimitNow(){
    float limit = 0.0f;
    OcppTimestamp validTo = OcppTimestamp(); //not needed
    auto& tNow = context.getOcppModel().getOcppTime().getOcppTimestampNow();
    inferenceLimit(tNow, &limit, &validTo);
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
void SmartChargingService::inferenceLimit(const OcppTimestamp &t, float *limitOutParam, OcppTimestamp *validToOutParam){
    OcppTimestamp validToMin = MAX_TIME;
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
    for (int i = CHARGEPROFILEMAXSTACKLEVEL - 1; i >= 0; i--) {
        if (!TxProfile[i])
            continue;
        if (!TxProfile[i]->checkTransactionAssignment(chargingSessionTransactionID, *sRmtProfileId))
            continue;
        OcppTimestamp nextChange = MAX_TIME;
        limit_defined_tx = TxProfile[i]->inferenceLimit(t, chargingSessionStart, &limit_tx, &nextChange);
        if (nextChange < validToMin)
            validToMin = nextChange; //nextChange is always >= t here
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
        OcppTimestamp nextChange = MAX_TIME;
        limit_defined_txdef = TxDefaultProfile[i]->inferenceLimit(t, chargingSessionStart, &limit_txdef, &nextChange);
        if (nextChange < validToMin)
            validToMin = nextChange; //nextChange is always >= t here
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
        OcppTimestamp nextChange = MAX_TIME;
        limit_defined_cpmax = ChargePointMaxProfile[i]->inferenceLimit(t, chargingSessionStart, &limit_cpmax, &nextChange);
        if (nextChange < validToMin)
            validToMin = nextChange; //nextChange is always >= t here
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

ChargingSchedule *SmartChargingService::getCompositeSchedule(int connectorId, otime_t duration){
    auto& startSchedule = context.getOcppModel().getOcppTime().getOcppTimestampNow();
    ChargingSchedule *result = new ChargingSchedule(startSchedule, duration);
    OcppTimestamp periodBegin = OcppTimestamp(startSchedule);
    OcppTimestamp periodStop = OcppTimestamp(startSchedule);
    while (periodBegin - startSchedule < duration) {
        float limit = 0.f;
        inferenceLimit(periodBegin, &limit, &periodStop);
        auto p = std::unique_ptr<ChargingSchedulePeriod>(new ChargingSchedulePeriod(periodBegin - startSchedule, limit));
        if (!result->addChargingSchedulePeriod(std::move(p))) {
            break;
        }
        periodBegin = periodStop;
    }
    return result;
}

void SmartChargingService::refreshChargingSessionState() {
    if (!context.getOcppModel().getConnectorStatus(SINGLE_CONNECTOR_ID)) {
        return; //charging session state does not apply
    }

    auto connector = context.getOcppModel().getConnectorStatus(SINGLE_CONNECTOR_ID);

    if (!chargingSessionStateInitialized) {
        chargingSessionStateInitialized = true;

        chargingSessionStart.setTime(*txStartTime);
        chargingSessionTransactionID = connector->getTransactionId();
        sessionIdTagRev = connector->getSessionWriteCount();
        sRmtProfileIdRev = sRmtProfileId->getValueRevision();

        //fuzzy check if session engaged at reboot (during first loop run)
        auto chargingSessionStartCheck = MAX_TIME;
        chargingSessionStartCheck -= 1000000; 
        if (chargingSessionStart >= chargingSessionStartCheck) {
            //charging session start lies in future -> null-value -> no charging session before reboot
            chargingSessionTransactionID = -1;
        }
    }

    if (connector->getTransactionId() != chargingSessionTransactionID) {
        //transition!

        bool txStartUpdated = false;
        if (chargingSessionTransactionID != 0 && connector->getTransactionId() >= 0) {
            chargingSessionStart = context.getOcppModel().getOcppTime().getOcppTimestampNow();
            txStartUpdated = true;
        } else if (chargingSessionTransactionID >= 0 && connector->getTransactionId() < 0) {
            chargingSessionStart = MAX_TIME;
            txStartUpdated = true;
        }

        if (txStartUpdated) {
            char timestamp [JSONDATE_LENGTH + 1] = {'\0'};
            chargingSessionStart.toJsonString(timestamp, JSONDATE_LENGTH + 1);
            *txStartTime = timestamp;
            configuration_save();
        }

        nextChange = context.getOcppModel().getOcppTime().getOcppTimestampNow();
    }
  
    if (*sRmtProfileId >= 0 && //Remote profile set? Check if to delete
            (!connector->getSessionIdTag()    //Always delete Rmt profile if there is no session
            || (sessionIdTagRev != connector->getSessionWriteCount() && sRmtProfileIdRev == sRmtProfileId->getValueRevision()))) {
                                               //Alternaternively delete if session state has been overwritten
        
        //after RemoteTx session expired, clean charging profile
        int clearProfileId = *sRmtProfileId;
        bool ret = clearChargingProfile([clearProfileId] (int id, int, ChargingProfilePurposeType, int) {
            return id == clearProfileId;
        });
        (void)ret;

        AO_DBG_DEBUG("Clearing RmtTx Charging Profile after session expiry: %s", ret ? "success" : "already cleared");

        *sRmtProfileId = -1;
    }

    chargingSessionTransactionID = connector->getTransactionId();
    sessionIdTagRev = connector->getSessionWriteCount();
    sRmtProfileIdRev = sRmtProfileId->getValueRevision();
}

void SmartChargingService::setChargingProfile(JsonObject json) {
    ChargingProfile *pointer = updateProfileStack(json);
    if (pointer)
        writeProfileToFlash(json, pointer);
}

ChargingProfile *SmartChargingService::updateProfileStack(JsonObject json){
    ChargingProfile *chargingProfile = new ChargingProfile(json);

    if (AO_DBG_LEVEL >= AO_DL_VERBOSE) {
        AO_DBG_VERBOSE("Charging Profile internal model:");
        chargingProfile->printProfile();
    }

    int stackLevel = chargingProfile->getStackLevel();
    if (stackLevel >= CHARGEPROFILEMAXSTACKLEVEL || stackLevel < 0) {
        AO_DBG_ERR("Stacklevel of Charging Profile is smaller or greater than CHARGEPROFILEMAXSTACKLEVEL");
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
    nextChange = context.getOcppModel().getOcppTime().getOcppTimestampNow();

    return chargingProfile;
}

bool SmartChargingService::clearChargingProfile(const std::function<bool(int, int, ChargingProfilePurposeType, int)>& filter) {
    int nMatches = 0;

    ChargingProfile **profileStacks [] = {ChargePointMaxProfile, TxDefaultProfile, TxProfile};

    for (ChargingProfile **profileStack : profileStacks) {
        for (int iLevel = 0; iLevel < CHARGEPROFILEMAXSTACKLEVEL; iLevel++) {
            ChargingProfile *chargingProfile = profileStack[iLevel];
            if (chargingProfile == NULL)
                continue;

            //                                                               -1: multiple connectors are not supported yet for Smart Charging
            bool tbCleared = filter(chargingProfile->getChargingProfileId(), -1, chargingProfile->getChargingProfilePurpose(), iLevel);

            if (tbCleared) {
                nMatches++;

#ifndef AO_DEACTIVATE_FLASH
                if (filesystemOpt.accessAllowed()) {
                    char fn [PROFILE_FN_MAXSIZE] = {'\0'};

                    switch (chargingProfile->getChargingProfilePurpose()) {
                        case (ChargingProfilePurposeType::ChargePointMaxProfile):
                            snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "CpMaxProfile-%d" PROFILE_FN_SUFFIX, chargingProfile->getStackLevel());
                            break;
                        case (ChargingProfilePurposeType::TxDefaultProfile):
                            snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "TxDefProfile-%d" PROFILE_FN_SUFFIX, chargingProfile->getStackLevel());
                            break;
                        case (ChargingProfilePurposeType::TxProfile):
                            snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "TxProfile-%d"    PROFILE_FN_SUFFIX, chargingProfile->getStackLevel());
                            break;
                    }

                    if (USE_FS.exists(fn)) {
                        USE_FS.remove(fn);
                    }
                } else {
                    AO_DBG_DEBUG("Prohibit access to FS");
                }
#endif
                delete chargingProfile;
                profileStack[iLevel] = nullptr;
            }
        }
    }

    /**
     * Invalidate the last limit inference by setting the nextChange to now. By the next loop()-call, the limit
     * and nextChange will be recalculated and onLimitChanged will be called.
     */
    nextChange = context.getOcppModel().getOcppTime().getOcppTimestampNow();

    return nMatches > 0;
}

bool SmartChargingService::writeProfileToFlash(JsonObject json, ChargingProfile *chargingProfile) {
#ifndef AO_DEACTIVATE_FLASH

    if (!filesystemOpt.accessAllowed()) {
        AO_DBG_DEBUG("Prohibit access to FS");
        return true;
    }

    char fn [PROFILE_FN_MAXSIZE] = {'\0'};

    switch (chargingProfile->getChargingProfilePurpose()) {
        case (ChargingProfilePurposeType::ChargePointMaxProfile):
            snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "CpMaxProfile-%d" PROFILE_FN_SUFFIX, chargingProfile->getStackLevel());
            break;
        case (ChargingProfilePurposeType::TxDefaultProfile):
            snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "TxDefProfile-%d" PROFILE_FN_SUFFIX, chargingProfile->getStackLevel());
            break;
        case (ChargingProfilePurposeType::TxProfile):
            snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "TxProfile-%d"    PROFILE_FN_SUFFIX, chargingProfile->getStackLevel());
            break;
    }

    if (USE_FS.exists(fn)) {
        USE_FS.remove(fn);
    }

    File file = USE_FS.open(fn, "w");

    if (!file) {
        AO_DBG_ERR("Unable to save: could not save profile: %s", fn);
        return false;
    }

    // Serialize JSON to file
    if (serializeJson(json, file) == 0) {
        AO_DBG_ERR("Unable to save: could not serialize JSON for profile: %s", fn);
        file.close();
        return false;
    }

    //success
    file.close();

    AO_DBG_DEBUG("Saving profile successful");

#endif //ndef AO_DEACTIVATE_FLASH
    return true;
}

bool SmartChargingService::loadProfiles() {

    bool success = true;

#ifndef AO_DEACTIVATE_FLASH
    if (!filesystemOpt.accessAllowed()) {
        AO_DBG_DEBUG("Prohibit access to FS");
        return true;
    }

    ChargingProfilePurposeType purposes[] = {ChargingProfilePurposeType::ChargePointMaxProfile, ChargingProfilePurposeType::TxDefaultProfile, ChargingProfilePurposeType::TxProfile};

    char fn [PROFILE_FN_MAXSIZE] = {'\0'};

    for (const ChargingProfilePurposeType purpose : purposes) {

        for (int iLevel = 0; iLevel < CHARGEPROFILEMAXSTACKLEVEL; iLevel++) {

            switch (purpose) {
                case (ChargingProfilePurposeType::ChargePointMaxProfile):
                    snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "CpMaxProfile-%d" PROFILE_FN_SUFFIX, iLevel);
                    break;
                case (ChargingProfilePurposeType::TxDefaultProfile):
                    snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "TxDefProfile-%d" PROFILE_FN_SUFFIX, iLevel);
                    break;
                case (ChargingProfilePurposeType::TxProfile):
                    snprintf(fn, PROFILE_FN_MAXSIZE, PROFILE_FN_PREFIX "TxProfile-%d"    PROFILE_FN_SUFFIX, iLevel);
                    break;
            }

            if (!USE_FS.exists(fn)) {
                continue; //There is not a profile on the stack iStack with stacklevel iLevel. Normal case, just continue.
            }
            
            File file = USE_FS.open(fn, "r");

            if (file) {
                AO_DBG_DEBUG("Load profile from file: %s", fn);
            } else {
                AO_DBG_ERR("Unable to initialize: could not open file for profile: %s", fn);
                success = false;
                continue;
            }

            if (!file.available()) {
                AO_DBG_ERR("Unable to initialize: empty file for profile: %s", fn);
                file.close();
                success = false;
                continue;
            }

            int file_size = file.size();

            if (file_size < 2) {
                AO_DBG_ERR("Unable to initialize: too short for json: %s", fn);
                success = false;
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
                        AO_DBG_ERR("Unable to initialize: invalid json in file: %s", fn);
                        success = false;
                        break;
                    case DeserializationError::NoMemory:
                        increaseCapacity = true;
                        error = false;
                        break;
                    default:
                        AO_DBG_ERR("Unable to initialize: error in file: %s", fn);
                        success = false;
                        break;
                }

                if (error) {
                    break;
                }

                if (increaseCapacity) {
                    capacity *= 3;
                    capacity /= 2;
                    file.seek(0, SeekSet); //rewind file to beginning
                    AO_DBG_DEBUG("Initialization: increase JsonCapacity to %zu for file: %s", capacity, fn);
                    continue;
                }

                JsonObject profileJson = profileDoc.as<JsonObject>();
                updateProfileStack(profileJson);

                profileDoc.clear();
                break;
            }

            file.close();
        }
    }

#endif //ndef AO_DEACTIVATE_FLASH
    return success;
}
