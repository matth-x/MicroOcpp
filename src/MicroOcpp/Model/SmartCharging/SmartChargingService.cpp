// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Operations/ClearChargingProfile.h>
#include <MicroOcpp/Operations/GetCompositeSchedule.h>
#include <MicroOcpp/Operations/SetChargingProfile.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

namespace MicroOcpp {
template <class T>
const T& minIfDefined(const T& a, const T& b) {
    return (a >= (T)0 && b >= (T)0) ? std::min(a, b) : std::max(a, b);
}

//filesystem-related helper functions
namespace SmartChargingServiceUtils {
bool printProfileFileName(char *out, size_t bufsize, unsigned int evseId, ChargingProfilePurposeType purpose, unsigned int stackLevel);
bool storeProfile(MO_FilesystemAdapter *filesystem, Clock& clock, int ocppVersion, unsigned int evseId, ChargingProfile *chargingProfile);
bool removeProfile(MO_FilesystemAdapter *filesystem, unsigned int evseId, ChargingProfilePurposeType purpose, unsigned int stackLevel);
} //namespace SmartChargingServiceUtils 
} //namespace MicroOcpp

using namespace::MicroOcpp;

SmartChargingServiceEvse::SmartChargingServiceEvse(Context& context, SmartChargingService& scService, unsigned int evseId) :
        MemoryManaged("v16.SmartCharging.SmartChargingServiceEvse"), context(context), clock(context.getClock()), scService{scService}, evseId{evseId} {
    
    mo_chargeRate_init(&trackLimitOutput);
}

SmartChargingServiceEvse::~SmartChargingServiceEvse() {
    for (size_t i = 0; i < MO_CHARGEPROFILESTACK_SIZE; i++) {
        delete txDefaultProfile[i];
        txDefaultProfile[i] = nullptr;
    }
    for (size_t i = 0; i < MO_CHARGEPROFILESTACK_SIZE; i++) {
        delete txProfile[i];
        txProfile[i] = nullptr;
    }
}

/*
 * limitOut: the calculated maximum charge rate at the moment, or the default value if no limit is defined
 * validToOut: The begin of the next SmartCharging restriction after time t
 */
void SmartChargingServiceEvse::calculateLimit(int32_t unixTime, int32_t sessionDurationSecs, MO_ChargeRate& limitOut, int32_t& nextChangeSecsOut) {

    //initialize output parameters with the default values
    mo_chargeRate_init(&limitOut);
    nextChangeSecsOut = 365 * 24 * 3600;

    bool txLimitDefined = false;
    MO_ChargeRate txLimit;
    mo_chargeRate_init(&txLimit);

    //first, check if txProfile is defined and limits charging
    for (int i = MO_ChargeProfileMaxStackLevel; i >= 0; i--) {
        if (txProfile[i]) {

            bool txMatch = false;

            #if MO_ENABLE_V16
            if (scService.ocppVersion == MO_OCPP_V16) {
                txMatch = (trackTxRmtProfileId >= 0 && trackTxRmtProfileId == txProfile[i]->chargingProfileId) ||
                             txProfile[i]->transactionId16 < 0 ||
                             trackTransactionId16 == txProfile[i]->transactionId16;
            }
            #endif //MO_ENABLE_V16
            #if MO_ENABLE_V201
            if (scService.ocppVersion == MO_OCPP_V201) {
                txMatch = (trackTxRmtProfileId >= 0 && trackTxRmtProfileId == txProfile[i]->chargingProfileId) ||
                             !*txProfile[i]->transactionId201 < 0 ||
                             !strcmp(trackTransactionId201, txProfile[i]->transactionId201);
            }
            #endif //MO_ENABLE_V201

            if (txMatch) {
                MO_ChargeRate crOut;
                mo_chargeRate_init(&crOut);
                bool defined = txProfile[i]->calculateLimit(unixTime, sessionDurationSecs, crOut, nextChangeSecsOut);
                if (defined) {
                    txLimitDefined = true;
                    txLimit = crOut;
                    break;
                }
            }
        }
    }

    //if no txProfile limits charging, check the txDefaultProfiles for this connector
    if (!txLimitDefined && sessionDurationSecs >= 0) {
        for (int i = MO_ChargeProfileMaxStackLevel; i >= 0; i--) {
            if (txDefaultProfile[i]) {
                MO_ChargeRate crOut;
                mo_chargeRate_init(&crOut);
                bool defined = txDefaultProfile[i]->calculateLimit(unixTime, sessionDurationSecs, crOut, nextChangeSecsOut);
                if (defined) {
                    txLimitDefined = true;
                    txLimit = crOut;
                    break;
                }
            }
        }
    }

    //if no appropriate txDefaultProfile is set for this connector, search in the general txDefaultProfiles
    if (!txLimitDefined && sessionDurationSecs >= 0) {
        for (int i = MO_ChargeProfileMaxStackLevel; i >= 0; i--) {
            if (scService.chargePointMaxProfile[i]) {
                MO_ChargeRate crOut;
                mo_chargeRate_init(&crOut);
                bool defined = scService.chargePointMaxProfile[i]->calculateLimit(unixTime, sessionDurationSecs, crOut, nextChangeSecsOut);
                if (defined) {
                    txLimitDefined = true;
                    txLimit = crOut;
                    break;
                }
            }
        }
    }

    MO_ChargeRate cpLimit;
    mo_chargeRate_init(&cpLimit);

    //the calculated maximum charge rate is also limited by the ChargePointMaxProfiles
    for (int i = MO_ChargeProfileMaxStackLevel; i >= 0; i--) {
        if (scService.chargePointMaxProfile[i]) {
            MO_ChargeRate crOut;
            mo_chargeRate_init(&crOut);
            bool defined = scService.chargePointMaxProfile[i]->calculateLimit(unixTime, sessionDurationSecs, crOut, nextChangeSecsOut);
            if (defined) {
                cpLimit = crOut;
                break;
            }
        }
    }

    //apply ChargePointMaxProfile value to calculated limit:
    //determine the minimum of both values if at least one is defined, otherwise assign -1 for undefined
    limitOut.power = minIfDefined(txLimit.power, cpLimit.power);
    limitOut.current = minIfDefined(txLimit.current, cpLimit.current);
    limitOut.numberPhases = minIfDefined(txLimit.numberPhases, cpLimit.numberPhases);

    #if MO_ENABLE_V201
    if (cpLimit.phaseToUse >= 0) {
        //ChargePoint Schedule always takes precedence for phaseToUse
        limitOut.phaseToUse = cpLimit.phaseToUse;
    } else {
        limitOut.phaseToUse = txLimit.phaseToUse; //note: can be -1 for undefined
    }
    #endif
}

void SmartChargingServiceEvse::trackTransaction() {

    bool update = false;

    #if MO_ENABLE_V16
    if (scService.ocppVersion == MO_OCPP_V16) {
        auto txSvc = context.getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        auto tx = txSvcEvse ? txSvcEvse->getTransaction() : nullptr;

        if (tx) {
            if (tx->getTxProfileId() != trackTxRmtProfileId) {
                update = true;
                trackTxRmtProfileId = tx->getTxProfileId();
            }
            int32_t dtTxStart;
            if (tx->getStartSync().isRequested() && clock.delta(tx->getStartTimestamp(), trackTxStart, dtTxStart) && dtTxStart != 0) {
                update = true;
                trackTxStart = tx->getStartTimestamp();
            }
            if (tx->getStartSync().isConfirmed() && tx->getTransactionId() != trackTransactionId16) {
                update = true;
                trackTransactionId16 = tx->getTransactionId();
            }
        } else {
            //check if transaction has just been completed
            if (trackTxRmtProfileId >= 0 || trackTxStart.isDefined() || trackTransactionId16 >= 0) {
                //yes, clear data
                update = true;
                trackTxRmtProfileId = -1;
                trackTxStart = Timestamp();
                trackTransactionId16 = -1;

                clearChargingProfile(-1, ChargingProfilePurposeType::TxProfile, -1);
            }
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (scService.ocppVersion == MO_OCPP_V201) {
        auto txService = context.getModel201().getTransactionService();
        auto txServiceEvse = txService ? txService->getEvse(evseId) : nullptr;
        auto tx = txServiceEvse ? txServiceEvse->getTransaction() : nullptr;

        if (tx) {
            if (tx->txProfileId != trackTxRmtProfileId) {
                update = true;
                trackTxRmtProfileId = tx->txProfileId;
            }
            if (tx->started) {
                int32_t dt;
                if (!clock.delta(tx->startTimestamp, trackTxStart, dt)) {
                    dt = -1; //force update
                }
                if (dt != 0) {
                    update = true;
                    trackTxStart = tx->startTimestamp;
                }
            }
            if (strcmp(tx->transactionId, trackTransactionId201)) {
                update = true;
                auto ret = snprintf(trackTransactionId201, sizeof(trackTransactionId201), "%s", tx->transactionId);
                if (ret < 0 || ret >= sizeof(trackTransactionId201)) {
                    MO_DBG_ERR("snprintf: %i", ret);
                    memset(trackTransactionId201, 0, sizeof(trackTransactionId201));
                }
            }
        } else {
            //check if transaction has just been completed
            if (trackTxRmtProfileId >= 0 || trackTxStart.isDefined() || trackTransactionId16 >= 0) {
                //yes, clear data
                update = true;
                trackTxRmtProfileId = -1;
                trackTxStart = Timestamp();
                memset(trackTransactionId201, 0, sizeof(trackTransactionId201));

                clearChargingProfile(-1, ChargingProfilePurposeType::TxProfile, -1);
            }
        }
    }
    #endif //MO_ENABLE_V201

    if (update) {
        nextChange = clock.now(); //will refresh limit calculation
    }
}

void SmartChargingServiceEvse::loop(){

    trackTransaction();

    /**
     * check if to call onLimitChange
     */
    auto& tnow = clock.now();

    int32_t dtNextChange;
    if (!clock.delta(tnow, nextChange, dtNextChange)) {
        dtNextChange = 0;
    }

    if (dtNextChange >= 0) {

        int32_t unixTime = -1;
        if (!clock.toUnixTime(tnow, unixTime)) {
            unixTime = 0; //undefined
        }

        int32_t sessionDurationSecs = -1;
        if (!trackTxStart.isDefined() || !clock.delta(tnow, trackTxStart, sessionDurationSecs)) {
            sessionDurationSecs = -1;
        }

        MO_ChargeRate limit;
        int32_t nextChangeSecs;

        calculateLimit(unixTime, sessionDurationSecs, limit, nextChangeSecs);

        //reset nextChange with nextChangeSecs
        nextChange = tnow;
        clock.add(nextChange, nextChangeSecs);

#if MO_DBG_LEVEL >= MO_DL_INFO
        {
            char timestamp1[MO_JSONDATE_SIZE] = {'\0'};
            if (!clock.toJsonString(tnow, timestamp1, sizeof(timestamp1))) {
                timestamp1[0] = '\0';
            }
            char timestamp2[MO_JSONDATE_SIZE] = {'\0'};
            if (!clock.toJsonString(nextChange, timestamp2, sizeof(timestamp2))) {
                timestamp1[0] = '\0';
            }
            MO_DBG_INFO("New limit for connector %u, scheduled at = %s, nextChange = %s, limit = {%.1f, %.1f, %i}",
                                evseId,
                                timestamp1, timestamp2,
                                limit.power,
                                limit.current,
                                limit.numberPhases);
        }
#endif

        if (!mo_chargeRate_equals(&trackLimitOutput, &limit)) {
            if (limitOutput) {

                limitOutput(limit, evseId, limitOutputUserData);
                trackLimitOutput = limit;
            }
        }
    }
}

void SmartChargingServiceEvse::setSmartChargingOutput(void (*limitOutput)(MO_ChargeRate limit, unsigned int evseId, void *userData), bool powerSupported, bool currentSupported, bool phases3to1Supported, bool phaseSwitchingSupported, void *userData) {
    if (this->limitOutput) {
        MO_DBG_WARN("replacing existing SmartChargingOutput");
    }
    this->limitOutput = limitOutput;
    this->limitOutputUserData = userData;

    scService.powerSupported |= powerSupported;
    scService.currentSupported |= currentSupported;
    scService.phases3to1Supported |= phases3to1Supported;
    #if MO_ENABLE_V201
    scService.phaseSwitchingSupported |= phaseSwitchingSupported;
    #endif //MO_ENABLE_V201
}

bool SmartChargingServiceEvse::updateProfile(std::unique_ptr<ChargingProfile> chargingProfile, bool updateFile) {
    
    ChargingProfile **stack = nullptr;

    switch (chargingProfile->chargingProfilePurpose) {
        case ChargingProfilePurposeType::TxDefaultProfile:
            stack = txDefaultProfile;
            break;
        case ChargingProfilePurposeType::TxProfile:
            stack = txProfile;
            break;
        default:
            break;
    }

    if (!stack) {
        MO_DBG_ERR("invalid args");
        return false;
    }
    
    if (updateFile && scService.filesystem) {
        if (!SmartChargingServiceUtils::storeProfile(scService.filesystem, clock, scService.ocppVersion, evseId, chargingProfile.get())) {
            MO_DBG_ERR("fs error");
            return false;
        }
    }
    

    int stackLevel = chargingProfile->stackLevel; //already validated

    stack[stackLevel] = chargingProfile.release();

    return true;
}

void SmartChargingServiceEvse::notifyProfilesUpdated() {
    nextChange = clock.now();
}

bool SmartChargingServiceEvse::clearChargingProfile(int chargingProfileId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
    bool found = false;

    ChargingProfilePurposeType purposes[] = {ChargingProfilePurposeType::TxProfile, ChargingProfilePurposeType::TxDefaultProfile};
    for (unsigned int p = 0; p < sizeof(purposes) / sizeof(purposes[0]); p++) {
        ChargingProfilePurposeType purpose = purposes[p];
        if (chargingProfilePurpose != ChargingProfilePurposeType::UNDEFINED && chargingProfilePurpose != purpose) {
            continue;
        }

        ChargingProfile **stack = nullptr;
        if (purpose == ChargingProfilePurposeType::TxProfile) {
            stack = txProfile;
        } else if (purpose == ChargingProfilePurposeType::TxDefaultProfile) {
            stack = txDefaultProfile;
        }

        for (size_t sLvl = 0; sLvl < MO_CHARGEPROFILESTACK_SIZE; sLvl++) {
            if (stackLevel >= 0 && (size_t)stackLevel != sLvl) {
                continue;
            }
            
            if (!stack[sLvl]) {
                // no profile installed at this stack and stackLevel
                continue;
            }

            if (chargingProfileId >= 0 && chargingProfileId != stack[sLvl]->chargingProfileId) {
                continue;
            }

            // this profile matches all filter criteria
            if (scService.filesystem) {
                SmartChargingServiceUtils::removeProfile(scService.filesystem, evseId, purpose, sLvl);
            }
            delete stack[sLvl];
            stack[sLvl] = nullptr;
            found = true;
        }
    }

    return found;
}

std::unique_ptr<ChargingSchedule> SmartChargingServiceEvse::getCompositeSchedule(int duration, ChargingRateUnitType unit) {
    
    int32_t nowUnixTime;
    if (!clock.toUnixTime(clock.now(), nowUnixTime)) {
        MO_DBG_ERR("internal error");
        return nullptr;
    }

    // dry run to measure Schedule size
    size_t periodsSize = 0;

    int32_t periodBegin = nowUnixTime;
    int32_t measuredDuration = 0;
    while (measuredDuration < duration && periodsSize < MO_ChargingScheduleMaxPeriods) {
        MO_ChargeRate limit;
        int32_t nextChangeSecs;

        calculateLimit(periodBegin, -1, limit, nextChangeSecs);

        periodBegin += nextChangeSecs;
        measuredDuration += nextChangeSecs;
        periodsSize++;
    }

    auto schedule = std::unique_ptr<ChargingSchedule>(new ChargingSchedule());
    if (!schedule || !schedule->resizeChargingSchedulePeriod(periodsSize)) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    schedule->duration = duration;
    schedule->startSchedule = nowUnixTime;
    schedule->chargingProfileKind = ChargingProfileKindType::Absolute;
    schedule->recurrencyKind = RecurrencyKindType::UNDEFINED;

    periodBegin = nowUnixTime;
    measuredDuration = 0;

    for (size_t i = 0; measuredDuration < duration && i < periodsSize; i++) {

        //calculate limit
        MO_ChargeRate limit;
        int32_t nextChangeSecs;

        calculateLimit(periodBegin, -1, limit, nextChangeSecs);

        //if the unit is still unspecified, guess by taking the unit of the first limit
        if (unit == ChargingRateUnitType::UNDEFINED) {
            if (limit.power > limit.current) {
                unit = ChargingRateUnitType::Watt;
            } else {
                unit = ChargingRateUnitType::Amp;
            }
        }

        float limit_opt = unit == ChargingRateUnitType::Watt ? limit.power : limit.current;
        schedule->chargingSchedulePeriod[i]->limit = (unit == ChargingRateUnitType::Watt) ? limit.power : limit.current,
        schedule->chargingSchedulePeriod[i]->numberPhases = limit.numberPhases;
        schedule->chargingSchedulePeriod[i]->startPeriod = measuredDuration;
        #if MO_ENABLE_V201
        schedule->chargingSchedulePeriod[i]->phaseToUse = limit.phaseToUse;
        #endif //MO_ENABLE_V201

        periodBegin += nextChangeSecs;
        measuredDuration += nextChangeSecs;
    }

    schedule->chargingRateUnit = unit;

    return schedule;
}

size_t SmartChargingServiceEvse::getChargingProfilesCount() {
    size_t chargingProfilesCount = 0;
    for (size_t i = 0; i < MO_CHARGEPROFILESTACK_SIZE; i++) {
        if (txDefaultProfile[i]) {
            chargingProfilesCount++;
        }
        if (txProfile[i]) {
            chargingProfilesCount++;
        }
    }
    return chargingProfilesCount;
}

SmartChargingService::SmartChargingService(Context& context)
      : MemoryManaged("v16.SmartCharging.SmartChargingService"), context(context), clock(context.getClock()) {

    mo_chargeRate_init(&trackLimitOutput);
}

SmartChargingService::~SmartChargingService() {
    for (unsigned int i = 0; i < MO_NUM_EVSEID; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
    for (size_t i = 0; i < MO_CHARGEPROFILESTACK_SIZE; i++) {
        delete chargePointMaxProfile[i];
        chargePointMaxProfile[i] = nullptr;
    }
    for (size_t i = 0; i < MO_CHARGEPROFILESTACK_SIZE; i++) {
        delete chargePointTxDefaultProfile[i];
        chargePointTxDefaultProfile[i] = nullptr;
    }
}

bool SmartChargingService::setup() {

    ocppVersion = context.getOcppVersion();

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {

    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {

    }
    #endif //MO_ENABLE_V201

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        auto configService = context.getModel16().getConfigurationService();
        if (!configService) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        const char *chargingScheduleAllowedChargingRateUnit = "";
        if (powerSupported && currentSupported) {
            chargingScheduleAllowedChargingRateUnit = "Current,Power";
        } else if (powerSupported) {
            chargingScheduleAllowedChargingRateUnit = "Power";
        } else if (currentSupported) {
            chargingScheduleAllowedChargingRateUnit = "Current";
        }
        configService->declareConfiguration<const char*>("ChargingScheduleAllowedChargingRateUnit", chargingScheduleAllowedChargingRateUnit, MO_CONFIGURATION_VOLATILE);

        configService->declareConfiguration<int>("ChargeProfileMaxStackLevel", MO_ChargeProfileMaxStackLevel, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
        configService->declareConfiguration<const char*>("ChargingScheduleAllowedChargingRateUnit", "", MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
        configService->declareConfiguration<int>("ChargingScheduleMaxPeriods", MO_ChargingScheduleMaxPeriods, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
        configService->declareConfiguration<int>("MaxChargingProfilesInstalled", MO_MaxChargingProfilesInstalled, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
        if (phases3to1Supported) {
            configService->declareConfiguration<bool>("ConnectorSwitch3to1PhaseSupported", phases3to1Supported, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {

        auto varService = context.getModel201().getVariableService();
        if (!varService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        chargingProfileEntriesInt201 = varService->declareVariable<int>("SmartChargingCtrlr", "ChargingProfileEntries", 0, Mutability::ReadOnly, false);
        if (!chargingProfileEntriesInt201) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        varService->declareVariable<bool>("SmartChargingCtrlr", "SmartChargingEnabled", true, Mutability::ReadOnly, false);
        if (phaseSwitchingSupported) {
            varService->declareVariable<bool>("SmartChargingCtrlr", "ACPhaseSwitchingSupported", phaseSwitchingSupported, Mutability::ReadOnly, false);
        }
        varService->declareVariable<int>("SmartChargingCtrlr", "ChargingProfileMaxStackLevel", MO_ChargeProfileMaxStackLevel, Mutability::ReadOnly, false);

        const char *chargingScheduleChargingRateUnit = "";
        if (powerSupported && currentSupported) {
            chargingScheduleChargingRateUnit = "A,W";
        } else if (powerSupported) {
            chargingScheduleChargingRateUnit = "W";
        } else if (currentSupported) {
            chargingScheduleChargingRateUnit = "A";
        }
        varService->declareVariable<const char*>("SmartChargingCtrlr", "ChargingScheduleChargingRateUnit", chargingScheduleChargingRateUnit, Mutability::ReadOnly, false);

        varService->declareVariable<int>("SmartChargingCtrlr", "PeriodsPerSchedule", MO_ChargingScheduleMaxPeriods, Mutability::ReadOnly, false);
        if (phases3to1Supported) {
            varService->declareVariable<bool>("SmartChargingCtrlr", "Phases3to1", phases3to1Supported, Mutability::ReadOnly, false);
        }
        varService->declareVariable<bool>("SmartChargingCtrlr", "SmartChargingAvailable", true, Mutability::ReadOnly, false);
    }
    #endif //MO_ENABLE_V201

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        context.getMessageService().registerOperation("ClearChargingProfile", [] (Context& context) -> Operation* {
            return new ClearChargingProfile(*context.getModel16().getSmartChargingService(), MO_OCPP_V16);});
        context.getMessageService().registerOperation("GetCompositeSchedule", [] (Context& context) -> Operation* {
            return new GetCompositeSchedule(context, *context.getModel16().getSmartChargingService());});
        context.getMessageService().registerOperation("SetChargingProfile", [] (Context& context) -> Operation* {
            return new SetChargingProfile(context, *context.getModel16().getSmartChargingService());});
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        context.getMessageService().registerOperation("ClearChargingProfile", [] (Context& context) -> Operation* {
            return new ClearChargingProfile(*context.getModel201().getSmartChargingService(), MO_OCPP_V201);});
        context.getMessageService().registerOperation("GetCompositeSchedule", [] (Context& context) -> Operation* {
            return new GetCompositeSchedule(context, *context.getModel201().getSmartChargingService());});
        context.getMessageService().registerOperation("SetChargingProfile", [] (Context& context) -> Operation* {
            return new SetChargingProfile(context, *context.getModel201().getSmartChargingService());});
    }
    #endif //MO_ENABLE_V201

    numEvseId = context.getModel16().getNumEvseId();
    for (unsigned int i = 1; i < numEvseId; i++) { //evseId 0 won't be populated
        if (!getEvse(i)) {
            MO_DBG_ERR("OOM");
            return false;
        }
    }

    if (!loadProfiles()) {
        MO_DBG_ERR("loadProfiles");
        return false;
    }

    return true;
}

SmartChargingServiceEvse *SmartChargingService::getEvse(unsigned int evseId) {
    if (evseId == 0 || evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new SmartChargingServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

bool SmartChargingService::updateProfile(unsigned int evseId, std::unique_ptr<ChargingProfile> chargingProfile, bool updateFile){

    if (evseId >= numEvseId || !chargingProfile) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    if (MO_DBG_LEVEL >= MO_DL_VERBOSE) {
        MO_DBG_VERBOSE("Charging Profile internal model:");
        chargingProfile->printProfile(clock);
    }

    int stackLevel = chargingProfile->stackLevel;
    if (stackLevel < 0 || stackLevel >= MO_CHARGEPROFILESTACK_SIZE) {
        MO_DBG_ERR("input validation failed");
        return false;
    }

    size_t chargingProfilesCount = getChargingProfilesCount();

    if (chargingProfilesCount >= MO_MaxChargingProfilesInstalled) {
        MO_DBG_WARN("number of maximum charging profiles exceeded");
        return false;
    }

    bool success = false;

    switch (chargingProfile->chargingProfilePurpose) {
        case ChargingProfilePurposeType::ChargePointMaxProfile:
            if (evseId != 0) {
                MO_DBG_WARN("invalid charging profile");
                return false;
            }
            if (updateFile && filesystem) {
                if (!SmartChargingServiceUtils::storeProfile(filesystem, clock, ocppVersion, evseId, chargingProfile.get())) {
                    MO_DBG_ERR("fs error");
                    return false;
                }
            }
            chargePointMaxProfile[stackLevel] = chargingProfile.release();
            success = true;
            break;
        case ChargingProfilePurposeType::TxDefaultProfile:
            if (evseId == 0) {
                if (updateFile && filesystem) {
                    if (!SmartChargingServiceUtils::storeProfile(filesystem, clock, ocppVersion, evseId, chargingProfile.get())) {
                        MO_DBG_ERR("fs error");
                        return false;
                    }
                }
                chargePointTxDefaultProfile[stackLevel] = chargingProfile.release();
                success = true;
            } else {
                success = evses[evseId]->updateProfile(std::move(chargingProfile), updateFile);
            }
            break;
        case ChargingProfilePurposeType::TxProfile:
            if (evseId == 0) {
                MO_DBG_WARN("invalid charging profile");
                return false;
            } else {
                success = evses[evseId]->updateProfile(std::move(chargingProfile), updateFile);
            }
            break;
        default:
            MO_DBG_ERR("input validation failed");
            return false;
    }

    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        chargingProfileEntriesInt201->setInt((int)getChargingProfilesCount());
    }
    #endif //MO_ENABLE_V201

    /**
     * Invalidate the last limit by setting the nextChange to now. By the next loop()-call, the limit
     * and nextChange will be recalculated and onLimitChanged will be called.
     */
    if (success) {
        nextChange = clock.now();
        for (unsigned int i = 1; i < numEvseId; i++) {
            evses[i]->notifyProfilesUpdated();
        }
    }

    return success;
}

bool SmartChargingService::loadProfiles() {

    bool success = true;

    if (!filesystem) {
        MO_DBG_DEBUG("no filesystem");
        return true; //not an error
    }

    ChargingProfilePurposeType purposes[] = {ChargingProfilePurposeType::ChargePointMaxProfile, ChargingProfilePurposeType::TxDefaultProfile, ChargingProfilePurposeType::TxProfile};

    char fname [MO_MAX_PATH_SIZE] = {'\0'};

    for (unsigned int p = 0; p < sizeof(purposes) / sizeof(purposes[0]); p++) {
        ChargingProfilePurposeType purpose = purposes[p];

        for (unsigned int cId = 0; cId < numEvseId; cId++) {
            if (cId > 0 && purpose == ChargingProfilePurposeType::ChargePointMaxProfile) {
                continue;
            }
            for (unsigned int iLevel = 0; iLevel < MO_ChargeProfileMaxStackLevel; iLevel++) {

                if (!SmartChargingServiceUtils::printProfileFileName(fname, sizeof(fname), cId, purpose, iLevel)) {
                    return false;
                }

                char path [MO_MAX_PATH_SIZE];
                if (!FilesystemUtils::printPath(filesystem, path, sizeof(path), fname)) {
                    MO_DBG_ERR("fname error: %s", fname);
                    return false;
                }

                JsonDoc doc (0);
                auto ret = FilesystemUtils::loadJson(filesystem, fname, doc, getMemoryTag());
                switch (ret) {
                    case FilesystemUtils::LoadStatus::Success:
                        break; //continue loading JSON
                    case FilesystemUtils::LoadStatus::FileNotFound:
                        break; //There is not a profile on the stack iStack with stacklevel iLevel. Normal case, just continue.
                    case FilesystemUtils::LoadStatus::ErrOOM:
                        MO_DBG_ERR("OOM");
                        return false;
                    case FilesystemUtils::LoadStatus::ErrFileCorruption:
                    case FilesystemUtils::LoadStatus::ErrOther: {
                            MO_DBG_ERR("profile corrupt: %s, remove", fname);
                            success = false;
                            filesystem->remove(path);
                        }
                        break;
                }

                if (ret != FilesystemUtils::LoadStatus::Success) {
                    continue;
                }

                auto chargingProfile = std::unique_ptr<ChargingProfile>(new ChargingProfile());
                if (!chargingProfile) {
                    MO_DBG_ERR("OOM");
                    return false;
                }

                bool valid = chargingProfile->parseJson(clock, ocppVersion, doc.as<JsonObject>());
                if (valid) {
                    valid = updateProfile(cId, std::move(chargingProfile), false);
                }

                if (!valid) {
                    success = false;
                    MO_DBG_ERR("profile corrupt: %s, remove", fname);
                    filesystem->remove(path);
                }
            }
        }
    }

    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        chargingProfileEntriesInt201->setInt((int)getChargingProfilesCount());
    }
    #endif //MO_ENABLE_V201

    return success;
}

size_t SmartChargingService::getChargingProfilesCount() {
    size_t chargingProfilesCount = 0;
    for (size_t i = 0; i < MO_CHARGEPROFILESTACK_SIZE; i++) {
        if (chargePointTxDefaultProfile[i]) {
            chargingProfilesCount++;
        }
        if (chargePointMaxProfile[i]) {
            chargingProfilesCount++;
        }
    }
    for (unsigned int i = 1; i < numEvseId; i++) {
        chargingProfilesCount += evses[i]->getChargingProfilesCount();
    }

    return chargingProfilesCount;
}

/*
 * limitOut: the calculated maximum charge rate at the moment, or the default value if no limit is defined
 * validToOut: The begin of the next SmartCharging restriction after time t
 */
void SmartChargingService::calculateLimit(int32_t unixTime, MO_ChargeRate& limitOut, int32_t& nextChangeSecsOut){
    
    //initialize output parameters with the default values
    mo_chargeRate_init(&limitOut);
    nextChangeSecsOut = 365 * 24 * 3600;

    //get ChargePointMaxProfile with the highest stackLevel
    for (int i = MO_ChargeProfileMaxStackLevel; i >= 0; i--) {
        if (chargePointMaxProfile[i]) {
            MO_ChargeRate crOut;
            bool defined = chargePointMaxProfile[i]->calculateLimit(unixTime, -1, crOut, nextChangeSecsOut);
            if (defined) {
                limitOut = crOut;
                break;
            }
        }
    }
}

void SmartChargingService::loop(){

    for (unsigned int i = 1; i < numEvseId; i++) {
        evses[i]->loop();
    }

    /**
     * check if to call onLimitChange
     */
    auto& tnow = clock.now();

    int32_t dtNextChange;
    if (!clock.delta(tnow, nextChange, dtNextChange)) {
        dtNextChange = 0;
    }

    if (dtNextChange >= 0) {

        int32_t unixTime = -1;
        if (!clock.toUnixTime(tnow, unixTime)) {
            unixTime = 0; //undefined
        }

        MO_ChargeRate limit;
        int32_t nextChangeSecs;

        calculateLimit(unixTime, limit, nextChangeSecs);

        //reset nextChange with nextChangeSecs
        nextChange = tnow;
        clock.add(nextChange, nextChangeSecs);

#if MO_DBG_LEVEL >= MO_DL_INFO
        {
            char timestamp1[MO_JSONDATE_SIZE] = {'\0'};
            if (!clock.toJsonString(tnow, timestamp1, sizeof(timestamp1))) {
                timestamp1[0] = '\0';
            }
            char timestamp2[MO_JSONDATE_SIZE] = {'\0'};
            if (!clock.toJsonString(nextChange, timestamp2, sizeof(timestamp2))) {
                timestamp1[0] = '\0';
            }
            MO_DBG_INFO("New limit for connector %u, scheduled at = %s, nextChange = %s, limit = {%.1f, %.1f, %i}",
                                0,
                                timestamp1, timestamp2,
                                limit.power,
                                limit.current,
                                limit.numberPhases);
        }
#endif

        if (!mo_chargeRate_equals(&trackLimitOutput, &limit)) {
            if (limitOutput) {

                limitOutput(limit, 0, limitOutputUserData);
                trackLimitOutput = limit;
            }
        }
    }
}

bool SmartChargingService::setSmartChargingOutput(unsigned int evseId, void (*limitOutput)(MO_ChargeRate limit, unsigned int evseId, void *userData), bool powerSupported, bool currentSupported, bool phases3to1Supported, bool phaseSwitchingSupported, void *userData) {
    if (evseId >= numEvseId || (evseId > 0 && !getEvse(evseId))) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    if (evseId == 0) {
        if (this->limitOutput) {
            MO_DBG_WARN("replacing existing SmartChargingOutput");
        }
        this->limitOutput = limitOutput;
        this->limitOutputUserData = userData;
    } else {
        getEvse(evseId)->setSmartChargingOutput(limitOutput, powerSupported, currentSupported, phases3to1Supported, phaseSwitchingSupported, userData);
    }

    this->powerSupported |= powerSupported;
    this->currentSupported |= currentSupported;
    this->phases3to1Supported |= phases3to1Supported;
    #if MO_ENABLE_V201
    this->phaseSwitchingSupported |= phaseSwitchingSupported;
    #endif //MO_ENABLE_V201

    return true;
}

bool SmartChargingService::setChargingProfile(unsigned int evseId, std::unique_ptr<ChargingProfile> chargingProfile) {

    if (evseId >= numEvseId || (evseId > 0 && !evses[evseId]) || !chargingProfile) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    if ((!currentSupported && chargingProfile->chargingSchedule.chargingRateUnit == ChargingRateUnitType::Amp) ||
            (!powerSupported && chargingProfile->chargingSchedule.chargingRateUnit == ChargingRateUnitType::Watt)) {
        MO_DBG_WARN("unsupported charge rate unit");
        return false;
    }

    int chargingProfileId = chargingProfile->chargingProfileId;
    clearChargingProfile(chargingProfileId, -1, ChargingProfilePurposeType::UNDEFINED, -1);

    return updateProfile(evseId, std::move(chargingProfile), true);
}

bool SmartChargingService::clearChargingProfile(int chargingProfileId, int evseId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
    bool found = false;

    // Enumerate all profiles, check for filter criteria and delete

    for (unsigned int cId = 0; cId < numEvseId; cId++) {
        if (evseId >= 0 && (unsigned int)evseId != cId) {
            continue;
        }

        if (cId > 0) {
            found |= evses[cId]->clearChargingProfile(chargingProfileId, chargingProfilePurpose, stackLevel);
        } else {
            ChargingProfilePurposeType purposes[] = {ChargingProfilePurposeType::ChargePointMaxProfile, ChargingProfilePurposeType::TxDefaultProfile};
            for (unsigned int p = 0; p < sizeof(purposes) / sizeof(purposes[0]); p++) {
                ChargingProfilePurposeType purpose = purposes[p];
                if (chargingProfilePurpose != ChargingProfilePurposeType::UNDEFINED && chargingProfilePurpose != purpose) {
                    continue;
                }

                ChargingProfile **stack = nullptr;
                if (purpose == ChargingProfilePurposeType::ChargePointMaxProfile) {
                    stack = chargePointMaxProfile;
                } else if (purpose == ChargingProfilePurposeType::TxDefaultProfile) {
                    stack = chargePointTxDefaultProfile;
                }
    
                for (size_t sLvl = 0; sLvl < MO_CHARGEPROFILESTACK_SIZE; sLvl++) {
                    if (stackLevel >= 0 && (size_t)stackLevel != sLvl) {
                        continue;
                    }
                    
                    if (!stack[sLvl]) {
                        // no profile installed at this stack and stackLevel
                        continue;
                    }

                    if (chargingProfileId >= 0 && chargingProfileId != stack[sLvl]->chargingProfileId) {
                        continue;
                    }

                    // this profile matches all filter criteria
                    if (filesystem) {
                        SmartChargingServiceUtils::removeProfile(filesystem, cId, purpose, sLvl);
                    }
                    delete stack[sLvl];
                    stack[sLvl] = nullptr;
                    found = true;
                }
            }
        }
    }

    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        chargingProfileEntriesInt201->setInt((int)getChargingProfilesCount());
    }
    #endif //MO_ENABLE_V201

    /**
     * Invalidate the last limit by setting the nextChange to now. By the next loop()-call, the limit
     * and nextChange will be recalculated and onLimitChanged will be called.
     */
    nextChange = clock.now();
    for (unsigned int i = 1; i < numEvseId; i++) {
        evses[i]->notifyProfilesUpdated();
    }

    return found;
}

std::unique_ptr<ChargingSchedule> SmartChargingService::getCompositeSchedule(unsigned int evseId, int duration, ChargingRateUnitType unit) {

    if (evseId >= numEvseId || (evseId > 0 && !evses[evseId])) {
        MO_DBG_ERR("invalid args");
        return nullptr;
    }

    if (unit == ChargingRateUnitType::UNDEFINED) {
        if (powerSupported && !currentSupported) {
            unit = ChargingRateUnitType::Watt;
        } else if (!powerSupported && currentSupported) {
            unit = ChargingRateUnitType::Amp;
        }
    }

    if (evseId > 0) {
        return evses[evseId]->getCompositeSchedule(duration, unit);
    }

    int32_t nowUnixTime;
    if (!clock.toUnixTime(clock.now(), nowUnixTime)) {
        MO_DBG_ERR("internal error");
        return nullptr;
    }

    // dry run to measure Schedule size
    size_t periodsSize = 0;

    int32_t periodBegin = nowUnixTime;
    int32_t measuredDuration = 0;
    while (measuredDuration < duration && periodsSize < MO_ChargingScheduleMaxPeriods) {
        MO_ChargeRate limit;
        int32_t nextChangeSecs;

        calculateLimit(periodBegin, limit, nextChangeSecs);

        periodBegin += nextChangeSecs;
        measuredDuration += nextChangeSecs;
        periodsSize++;
    }

    auto schedule = std::unique_ptr<ChargingSchedule>(new ChargingSchedule());
    if (!schedule || !schedule->resizeChargingSchedulePeriod(periodsSize)) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    schedule->duration = duration;
    schedule->startSchedule = nowUnixTime;
    schedule->chargingProfileKind = ChargingProfileKindType::Absolute;
    schedule->recurrencyKind = RecurrencyKindType::UNDEFINED;

    periodBegin = nowUnixTime;
    measuredDuration = 0;

    for (size_t i = 0; measuredDuration < duration && i < periodsSize; i++) {

        //calculate limit
        MO_ChargeRate limit;
        int32_t nextChangeSecs;

        calculateLimit(periodBegin, limit, nextChangeSecs);

        //if the unit is still unspecified, guess by taking the unit of the first limit
        if (unit == ChargingRateUnitType::UNDEFINED) {
            if (limit.power > limit.current) {
                unit = ChargingRateUnitType::Watt;
            } else {
                unit = ChargingRateUnitType::Amp;
            }
        }

        float limit_opt = (unit == ChargingRateUnitType::Watt) ? limit.power : limit.current;
        schedule->chargingSchedulePeriod[i]->limit = (unit == ChargingRateUnitType::Watt) ? limit.power : limit.current;
        schedule->chargingSchedulePeriod[i]->numberPhases = limit.numberPhases;
        schedule->chargingSchedulePeriod[i]->startPeriod = measuredDuration;
        #if MO_ENABLE_V201
        schedule->chargingSchedulePeriod[i]->phaseToUse = limit.phaseToUse;
        #endif //MO_ENABLE_V201

        periodBegin += nextChangeSecs;
        measuredDuration += nextChangeSecs;
    }

    schedule->chargingRateUnit = unit;

    return schedule;
}

bool SmartChargingServiceUtils::printProfileFileName(char *out, size_t bufsize, unsigned int evseId, ChargingProfilePurposeType purpose, unsigned int stackLevel) {
    int ret = 0;

    switch (purpose) {
        case ChargingProfilePurposeType::ChargePointMaxProfile:
            ret = snprintf(out, bufsize, "sc-cm-%.*u-%.*u.jsn", MO_NUM_EVSEID_DIGITS, 0, MO_ChargeProfileMaxStackLevel_digits, stackLevel);
            break;
        case ChargingProfilePurposeType::TxDefaultProfile:
            ret = snprintf(out, bufsize, "sc-td-%.*u-%.*u.jsn", MO_NUM_EVSEID_DIGITS, evseId, MO_ChargeProfileMaxStackLevel_digits, stackLevel);
            break;
        case ChargingProfilePurposeType::TxProfile:
            ret = snprintf(out, bufsize, "sc-tx-%.*u-%.*u.jsn", MO_NUM_EVSEID_DIGITS, evseId, MO_ChargeProfileMaxStackLevel_digits, stackLevel);
            break;
    }

    if (ret < 0 || (size_t) ret >= bufsize) {
        MO_DBG_ERR("fname error: %i", ret);
        return false;
    }

    return true;
}

bool SmartChargingServiceUtils::storeProfile(MO_FilesystemAdapter *filesystem, Clock& clock, int ocppVersion, unsigned int evseId, ChargingProfile *chargingProfile) {

    auto capacity = chargingProfile->getJsonCapacity(ocppVersion);
    auto chargingProfileJson = initJsonDoc("v16.SmartCharging.ChargingProfile", capacity);
    if (!chargingProfile->toJson(clock, ocppVersion, chargingProfileJson.as<JsonObject>())) {
        return false;
    }

    char fname [MO_MAX_PATH_SIZE] = {'\0'};

    if (!printProfileFileName(fname, MO_MAX_PATH_SIZE, evseId, chargingProfile->chargingProfilePurpose, chargingProfile->stackLevel)) {
        return false;
    }

    if (FilesystemUtils::storeJson(filesystem, fname, chargingProfileJson) != FilesystemUtils::StoreStatus::Success) {
        MO_DBG_ERR("failed to store %s", fname);
    }

    return true;
}

bool SmartChargingServiceUtils::removeProfile(MO_FilesystemAdapter *filesystem, unsigned int evseId, ChargingProfilePurposeType purpose, unsigned int stackLevel) {

    char fname [MO_MAX_PATH_SIZE] = {'\0'};

    if (!printProfileFileName(fname, MO_MAX_PATH_SIZE, evseId, purpose, stackLevel)) {
        return false;
    }

    char path [MO_MAX_PATH_SIZE];
    if (!FilesystemUtils::printPath(filesystem, path, sizeof(path), fname)) {
        return false;
    }

    return filesystem->remove(path);
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
