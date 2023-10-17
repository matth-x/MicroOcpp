// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Operations/ClearChargingProfile.h>
#include <MicroOcpp/Operations/GetCompositeSchedule.h>
#include <MicroOcpp/Operations/SetChargingProfile.h>
#include <MicroOcpp/Debug.h>

using namespace::MicroOcpp;

SmartChargingConnector::SmartChargingConnector(Model& model, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId, ProfileStack& ChargePointMaxProfile, ProfileStack& ChargePointTxDefaultProfile) :
        model(model), filesystem{filesystem}, connectorId{connectorId}, ChargePointMaxProfile(ChargePointMaxProfile), ChargePointTxDefaultProfile(ChargePointTxDefaultProfile) {
    
}

SmartChargingConnector::~SmartChargingConnector() {

}

/*
 * limitOut: the calculated maximum charge rate at the moment, or the default value if no limit is defined
 * validToOut: The begin of the next SmartCharging restriction after time t
 */
void SmartChargingConnector::calculateLimit(const Timestamp &t, ChargeRate& limitOut, Timestamp& validToOut) {

    //initialize output parameters with the default values
    limitOut = ChargeRate();
    validToOut = MAX_TIME;

    bool txLimitDefined = false;
    ChargeRate txLimit;

    //first, check if TxProfile is defined and limits charging
    for (int i = CHARGEPROFILEMAXSTACKLEVEL; i >= 0; i--) {
        if (TxProfile[i] && ((trackTxRmtProfileId >= 0 && trackTxRmtProfileId == TxProfile[i]->getChargingProfileId()) ||
                                TxProfile[i]->getTransactionId() < 0 ||
                                trackTxId == TxProfile[i]->getTransactionId())) {
            ChargeRate crOut;
            bool defined = TxProfile[i]->calculateLimit(t, trackTxStart, crOut, validToOut);
            if (defined) {
                txLimitDefined = true;
                txLimit = crOut;
                break;
            }
        }
    }

    //if no TxProfile limits charging, check the TxDefaultProfiles for this connector
    if (!txLimitDefined && trackTxStart < MAX_TIME) {
        for (int i = CHARGEPROFILEMAXSTACKLEVEL; i >= 0; i--) {
            if (TxDefaultProfile[i]) {
                ChargeRate crOut;
                bool defined = TxDefaultProfile[i]->calculateLimit(t, trackTxStart, crOut, validToOut);
                if (defined) {
                    txLimitDefined = true;
                    txLimit = crOut;
                    break;
                }
            }
        }
    }

    //if no appropriate TxDefaultProfile is set for this connector, search in the general TxDefaultProfiles
    if (!txLimitDefined && trackTxStart < MAX_TIME) {
        for (int i = CHARGEPROFILEMAXSTACKLEVEL; i >= 0; i--) {
            if (ChargePointTxDefaultProfile[i]) {
                ChargeRate crOut;
                bool defined = ChargePointTxDefaultProfile[i]->calculateLimit(t, trackTxStart, crOut, validToOut);
                if (defined) {
                    txLimitDefined = true;
                    txLimit = crOut;
                    break;
                }
            }
        }
    }

    ChargeRate cpLimit;

    //the calculated maximum charge rate is also limited by the ChargePointMaxProfiles
    for (int i = CHARGEPROFILEMAXSTACKLEVEL; i >= 0; i--) {
        if (ChargePointMaxProfile[i]) {
            ChargeRate crOut;
            bool defined = ChargePointMaxProfile[i]->calculateLimit(t, trackTxStart, crOut, validToOut);
            if (defined) {
                cpLimit = crOut;
                break;
            }
        }
    }

    //apply ChargePointMaxProfile value to calculated limit
    limitOut = chargeRate_min(txLimit, cpLimit);
}

void SmartChargingConnector::trackTransaction() {

    Transaction *tx = nullptr;
    if (model.getConnector(connectorId)) {
        tx = model.getConnector(connectorId)->getTransaction().get();
    }

    bool update = false;

    if (tx) {
        if (tx->getTxProfileId() != trackTxRmtProfileId) {
            update = true;
            trackTxRmtProfileId = tx->getTxProfileId();
        }
        if (tx->getStartSync().isRequested() && tx->getStartTimestamp() != trackTxStart) {
            update = true;
            trackTxStart = tx->getStartTimestamp();
        }
        if (tx->getStartSync().isConfirmed() && tx->getTransactionId() != trackTxId) {
            update = true;
            trackTxId = tx->getTransactionId();
        }
    } else {
        //check if transaction has just been completed
        if (trackTxRmtProfileId >= 0 || trackTxStart < MAX_TIME || trackTxId >= 0) {
            //yes, clear data
            update = true;
            trackTxRmtProfileId = -1;
            trackTxStart = MAX_TIME;
            trackTxId = -1;

            clearChargingProfile([this] (int, int connectorId, ChargingProfilePurposeType purpose, int) {
                return purpose == ChargingProfilePurposeType::TxProfile &&
                       (int) this->connectorId == connectorId;
            });
        }
    }

    if (update) {
        nextChange = model.getClock().now(); //will refresh limit calculation
    }
}

void SmartChargingConnector::loop(){

    trackTransaction();

    /**
     * check if to call onLimitChange
     */
    auto& tnow = model.getClock().now();

    if (tnow >= nextChange){

        ChargeRate limit;
        nextChange = MAX_TIME; //reset nextChange to default value and refresh it

        calculateLimit(tnow, limit, nextChange);

        if (trackLimitOutput != limit) {
            if (limitOutput) {

                limitOutput(
                    limit.power != std::numeric_limits<float>::max() ? limit.power : -1.f,
                    limit.current != std::numeric_limits<float>::max() ? limit.current : -1.f,
                    limit.nphases != std::numeric_limits<int>::max() ? limit.nphases : -1);
                trackLimitOutput = limit;
            }
        }
    }
}

void SmartChargingConnector::setSmartChargingOutput(std::function<void(float,float,int)> limitOutput) {
    if (this->limitOutput) {
        MO_DBG_WARN("replacing existing SmartChargingOutput");
        (void)0;
    }
    this->limitOutput = limitOutput;
}

ChargingProfile *SmartChargingConnector::updateProfiles(std::unique_ptr<ChargingProfile> chargingProfile) {
    
    int stackLevel = chargingProfile->getStackLevel(); //already validated
    
    switch (chargingProfile->getChargingProfilePurpose()) {
        case (ChargingProfilePurposeType::ChargePointMaxProfile):
            break;
        case (ChargingProfilePurposeType::TxDefaultProfile):
            TxDefaultProfile[stackLevel] = std::move(chargingProfile);
            return TxDefaultProfile[stackLevel].get();
        case (ChargingProfilePurposeType::TxProfile):
            TxProfile[stackLevel] = std::move(chargingProfile);
            return TxProfile[stackLevel].get();
    }

    MO_DBG_ERR("invalid args");
    return nullptr;
}

void SmartChargingConnector::notifyProfilesUpdated() {
    nextChange = model.getClock().now();
}

bool SmartChargingConnector::clearChargingProfile(const std::function<bool(int, int, ChargingProfilePurposeType, int)> filter) {
    bool found = false;

    ProfileStack *profileStacks [] = {&TxProfile, &TxDefaultProfile};

    for (auto stack : profileStacks) {
        for (size_t iLevel = 0; iLevel < stack->size(); iLevel++) {
            if (auto& profile = stack->at(iLevel)) {
                if (profile && filter(profile->getChargingProfileId(), connectorId, profile->getChargingProfilePurpose(), iLevel)) {
                    found = true;
                    SmartChargingServiceUtils::removeProfile(filesystem, connectorId, profile->getChargingProfilePurpose(), iLevel);
                    profile.reset();
                }
            }
        }
    }

    return found;
}

std::unique_ptr<ChargingSchedule> SmartChargingConnector::getCompositeSchedule(int duration, ChargingRateUnitType_Optional unit) {
    
    auto& startSchedule = model.getClock().now();

    auto schedule = std::unique_ptr<ChargingSchedule>(new ChargingSchedule());
    schedule->duration = duration;
    schedule->startSchedule = startSchedule;
    schedule->chargingProfileKind = ChargingProfileKindType::Absolute;
    schedule->recurrencyKind = RecurrencyKindType::NOT_SET;

    auto& periods = schedule->chargingSchedulePeriod;

    Timestamp periodBegin = Timestamp(startSchedule);
    Timestamp periodStop = Timestamp(startSchedule);

    while (periodBegin - startSchedule < duration && periods.size() < CHARGINGSCHEDULEMAXPERIODS) {

        //calculate limit
        ChargeRate limit;
        calculateLimit(periodBegin, limit, periodStop);

        //if the unit is still unspecified, guess by taking the unit of the first limit
        if (unit == ChargingRateUnitType_Optional::None) {
            if (limit.power < limit.current) {
                unit = ChargingRateUnitType_Optional::Watt;
            } else {
                unit = ChargingRateUnitType_Optional::Amp;
            }
        }

        periods.push_back(ChargingSchedulePeriod());
        float limit_opt = unit == ChargingRateUnitType_Optional::Watt ? limit.power : limit.current;
        periods.back().limit = limit_opt != std::numeric_limits<float>::max() ? limit_opt : -1.f,
        periods.back().numberPhases = limit.nphases != std::numeric_limits<int>::max() ? limit.nphases : -1;
        periods.back().startPeriod = periodBegin - startSchedule;
        
        periodBegin = periodStop;
    }

    if (unit == ChargingRateUnitType_Optional::Watt) {
        schedule->chargingRateUnit = ChargingRateUnitType::Watt;
    } else {
        schedule->chargingRateUnit = ChargingRateUnitType::Amp;
    }

    return schedule;
}

size_t SmartChargingConnector::getChargingProfilesCount() {
    size_t chargingProfilesCount = 0;
    for (size_t i = 0; i < CHARGEPROFILEMAXSTACKLEVEL + 1; i++) {
        if (ChargePointTxDefaultProfile[i]) {
            chargingProfilesCount++;
        }
        if (ChargePointMaxProfile[i]) {
            chargingProfilesCount++;
        }
    }
    return chargingProfilesCount;
}

SmartChargingConnector *SmartChargingService::getScConnectorById(unsigned int connectorId) {
    if (connectorId == 0) {
        return nullptr;
    }

    if (connectorId - 1 >= connectors.size()) {
        return nullptr;
    }

    return &connectors[connectorId-1];
}

SmartChargingService::SmartChargingService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int numConnectors)
      : context(context), filesystem{filesystem}, numConnectors(numConnectors) {
    
    for (unsigned int cId = 1; cId < numConnectors; cId++) {
        connectors.push_back(std::move(SmartChargingConnector(context.getModel(), filesystem, cId, ChargePointMaxProfile, ChargePointTxDefaultProfile)));
    }

    declareConfiguration<int>("ChargeProfileMaxStackLevel", CHARGEPROFILEMAXSTACKLEVEL, CONFIGURATION_VOLATILE, true);
    declareConfiguration<const char*>("ChargingScheduleAllowedChargingRateUnit", "", CONFIGURATION_VOLATILE);
    declareConfiguration<int>("ChargingScheduleMaxPeriods", CHARGINGSCHEDULEMAXPERIODS, CONFIGURATION_VOLATILE, true);
    declareConfiguration<int>("MaxChargingProfilesInstalled", MAXCHARGINGPROFILESINSTALLED, CONFIGURATION_VOLATILE, true);

    context.getOperationRegistry().registerOperation("ClearChargingProfile", [this] () {
        return new Ocpp16::ClearChargingProfile(*this);});
    context.getOperationRegistry().registerOperation("GetCompositeSchedule", [&context, this] () {
        return new Ocpp16::GetCompositeSchedule(context.getModel(), *this);});
    context.getOperationRegistry().registerOperation("SetChargingProfile", [&context, this] () {
        return new Ocpp16::SetChargingProfile(context.getModel(), *this);});

    loadProfiles();
}

SmartChargingService::~SmartChargingService() {

}

ChargingProfile *SmartChargingService::updateProfiles(unsigned int connectorId, std::unique_ptr<ChargingProfile> chargingProfile){

    if ((connectorId > 0 && !getScConnectorById(connectorId)) || !chargingProfile) {
        MO_DBG_ERR("invalid args");
        return nullptr;
    }

    if (MO_DBG_LEVEL >= MO_DL_VERBOSE) {
        MO_DBG_VERBOSE("Charging Profile internal model:");
        chargingProfile->printProfile();
    }

    int stackLevel = chargingProfile->getStackLevel();
    if (stackLevel< 0 || stackLevel >= CHARGEPROFILEMAXSTACKLEVEL + 1) {
        MO_DBG_ERR("input validation failed");
        return nullptr;
    }

    size_t chargingProfilesCount = 0;
    for (size_t i = 0; i < CHARGEPROFILEMAXSTACKLEVEL + 1; i++) {
        if (ChargePointTxDefaultProfile[i]) {
            chargingProfilesCount++;
        }
        if (ChargePointMaxProfile[i]) {
            chargingProfilesCount++;
        }
    }
    for (size_t i = 0; i < connectors.size(); i++) {
        chargingProfilesCount += connectors[i].getChargingProfilesCount();
    }

    if (chargingProfilesCount >= MAXCHARGINGPROFILESINSTALLED) {
        MO_DBG_WARN("number of maximum charging profiles exceeded");
        return nullptr;
    }

    ChargingProfile *res = nullptr;

    switch (chargingProfile->getChargingProfilePurpose()) {
        case (ChargingProfilePurposeType::ChargePointMaxProfile):
            if (connectorId != 0) {
                MO_DBG_WARN("invalid charging profile");
                return nullptr;
            }
            ChargePointMaxProfile[stackLevel] = std::move(chargingProfile);
            res = ChargePointMaxProfile[stackLevel].get();
            break;
        case (ChargingProfilePurposeType::TxDefaultProfile):
            if (connectorId == 0) {
                ChargePointTxDefaultProfile[stackLevel] = std::move(chargingProfile);
                res = ChargePointTxDefaultProfile[stackLevel].get();
            } else {
                if (!getScConnectorById(connectorId)) {
                    MO_DBG_WARN("invalid charging profile");
                    return nullptr;
                }
                res = getScConnectorById(connectorId)->updateProfiles(std::move(chargingProfile));
            }
            break;
        case (ChargingProfilePurposeType::TxProfile):
            if (!getScConnectorById(connectorId)) {
                MO_DBG_WARN("invalid charging profile");
                return nullptr;
            }
            res = getScConnectorById(connectorId)->updateProfiles(std::move(chargingProfile));
            break;
    }

    /**
     * Invalidate the last limit by setting the nextChange to now. By the next loop()-call, the limit
     * and nextChange will be recalculated and onLimitChanged will be called.
     */
    if (res) {
        nextChange = context.getModel().getClock().now();
        for (size_t i = 0; i < connectors.size(); i++) {
            connectors[i].notifyProfilesUpdated();
        }
    }

    return res;
}

bool SmartChargingService::loadProfiles() {

    bool success = true;

    if (!filesystem) {
        MO_DBG_DEBUG("no filesystem");
        return true; //not an error
    }

    ChargingProfilePurposeType purposes[] = {ChargingProfilePurposeType::ChargePointMaxProfile, ChargingProfilePurposeType::TxDefaultProfile, ChargingProfilePurposeType::TxProfile};

    char fn [MO_MAX_PATH_SIZE] = {'\0'};

    for (auto purpose : purposes) {
        for (unsigned int cId = 0; cId < numConnectors; cId++) {
            if (cId > 0 && purpose == ChargingProfilePurposeType::ChargePointMaxProfile) {
                continue;
            }
            for (unsigned int iLevel = 0; iLevel < CHARGEPROFILEMAXSTACKLEVEL; iLevel++) {

                if (!SmartChargingServiceUtils::printProfileFileName(fn, MO_MAX_PATH_SIZE, cId, purpose, iLevel)) {
                    return false;
                }

                size_t msize = 0;
                if (filesystem->stat(fn, &msize) != 0) {
                    continue; //There is not a profile on the stack iStack with stacklevel iLevel. Normal case, just continue.
                }

                auto profileDoc = FilesystemUtils::loadJson(filesystem, fn);
                if (!profileDoc) {
                    success = false;
                    MO_DBG_ERR("profile corrupt: %s, remove", fn);
                    filesystem->remove(fn);
                    continue;
                }

                JsonObject profileJson = profileDoc->as<JsonObject>();
                auto chargingProfile = loadChargingProfile(profileJson);

                bool valid = false;
                if (chargingProfile) {
                    valid = updateProfiles(cId, std::move(chargingProfile));
                }
                if (!valid) {
                    success = false;
                    MO_DBG_ERR("profile corrupt: %s, remove", fn);
                    filesystem->remove(fn);
                }
            }
        }
    }

    return success;
}

/*
 * limitOut: the calculated maximum charge rate at the moment, or the default value if no limit is defined
 * validToOut: The begin of the next SmartCharging restriction after time t
 */
void SmartChargingService::calculateLimit(const Timestamp &t, ChargeRate& limitOut, Timestamp& validToOut){
    
    //initialize output parameters with the default values
    limitOut = ChargeRate();
    validToOut = MAX_TIME;

    //get ChargePointMaxProfile with the highest stackLevel
    for (int i = CHARGEPROFILEMAXSTACKLEVEL; i >= 0; i--) {
        if (ChargePointMaxProfile[i]) {
            ChargeRate crOut;
            bool defined = ChargePointMaxProfile[i]->calculateLimit(t, crOut, validToOut);
            if (defined) {
                limitOut = crOut;
                break;
            }
        }
    }
}

void SmartChargingService::loop(){

    for (size_t i = 0; i < connectors.size(); i++) {
        connectors[i].loop();
    }

    /**
     * check if to call onLimitChange
     */
    auto& tnow = context.getModel().getClock().now();

    if (tnow >= nextChange){
        
        ChargeRate limit;
        nextChange = MAX_TIME; //reset nextChange to default value and refresh it

        calculateLimit(tnow, limit, nextChange);

#if (MO_DBG_LEVEL >= MO_DL_INFO)
        char timestamp1[JSONDATE_LENGTH + 1] = {'\0'};
        tnow.toJsonString(timestamp1, JSONDATE_LENGTH + 1);
        char timestamp2[JSONDATE_LENGTH + 1] = {'\0'};
        nextChange.toJsonString(timestamp2, JSONDATE_LENGTH + 1);
        MO_DBG_INFO("New limit for connector 1, scheduled at = %s, nextChange = %s, limit = {%f,%f,%i}",
                            timestamp1, timestamp2,
                            limit.power != std::numeric_limits<float>::max() ? limit.power : -1.f,
                            limit.current != std::numeric_limits<float>::max() ? limit.current : -1.f,
                            limit.nphases != std::numeric_limits<int>::max() ? limit.nphases : -1);
#endif

        if (trackLimitOutput != limit) {
            if (limitOutput) {

                limitOutput(
                    limit.power != std::numeric_limits<float>::max() ? limit.power : -1.f,
                    limit.current != std::numeric_limits<float>::max() ? limit.current : -1.f,
                    limit.nphases != std::numeric_limits<int>::max() ? limit.nphases : -1);
                trackLimitOutput = limit;
            }
        }
    }
}

void SmartChargingService::setSmartChargingOutput(unsigned int connectorId, std::function<void(float,float,int)> limitOutput) {
    if ((connectorId > 0 && !getScConnectorById(connectorId))) {
        MO_DBG_ERR("invalid args");
        return;
    }

    if (connectorId == 0) {
        if (this->limitOutput) {
            MO_DBG_WARN("replacing existing SmartChargingOutput");
            (void)0;
        }
        this->limitOutput = limitOutput;
    } else {
        getScConnectorById(connectorId)->setSmartChargingOutput(limitOutput);
    }
}

void SmartChargingService::updateAllowedChargingRateUnit(bool powerSupported, bool currentSupported) {
    if ((powerSupported != this->powerSupported) || (currentSupported != this->currentSupported)) {

        auto chargingScheduleAllowedChargingRateUnitString = declareConfiguration<const char*>("ChargingScheduleAllowedChargingRateUnit", "", CONFIGURATION_VOLATILE);
        if (chargingScheduleAllowedChargingRateUnitString) {
            if (powerSupported && currentSupported) {
                chargingScheduleAllowedChargingRateUnitString->setString("Current,Power");
            } else if (powerSupported) {
                chargingScheduleAllowedChargingRateUnitString->setString("Power");
            } else if (currentSupported) {
                chargingScheduleAllowedChargingRateUnitString->setString("Current");
            }
        }

        this->powerSupported = powerSupported;
        this->currentSupported = currentSupported;
    }
}

bool SmartChargingService::setChargingProfile(unsigned int connectorId, std::unique_ptr<ChargingProfile> chargingProfile) {

    if ((connectorId > 0 && !getScConnectorById(connectorId)) || !chargingProfile) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    int chargingProfileId = chargingProfile->getChargingProfileId();
    clearChargingProfile([chargingProfileId] (int id, int, ChargingProfilePurposeType, int) {
        return id == chargingProfileId;
    });

    bool success = false;

    auto profilePtr = updateProfiles(connectorId, std::move(chargingProfile));

    if (profilePtr) {
        success = SmartChargingServiceUtils::storeProfile(filesystem, connectorId, profilePtr);

        if (!success) {
            clearChargingProfile([chargingProfileId] (int id, int, ChargingProfilePurposeType, int) {
                return id == chargingProfileId;
            });
        }
    }

    return success;
}

bool SmartChargingService::clearChargingProfile(std::function<bool(int, int, ChargingProfilePurposeType, int)> filter) {
    bool found = false;

    for (size_t cId = 0; cId < connectors.size(); cId++) {
        found |= connectors[cId].clearChargingProfile(filter);
    }

    ProfileStack *profileStacks [] = {&ChargePointMaxProfile, &ChargePointTxDefaultProfile};

    for (auto stack : profileStacks) {
        for (size_t iLevel = 0; iLevel < stack->size(); iLevel++) {
            if (auto& profile = stack->at(iLevel)) {
                if (filter(profile->getChargingProfileId(), 0, profile->getChargingProfilePurpose(), iLevel)) {
                    found = true;
                    SmartChargingServiceUtils::removeProfile(filesystem, 0, profile->getChargingProfilePurpose(), iLevel);
                    profile.reset();
                }
            }
        }
    }

    /**
     * Invalidate the last limit by setting the nextChange to now. By the next loop()-call, the limit
     * and nextChange will be recalculated and onLimitChanged will be called.
     */
    nextChange = context.getModel().getClock().now();
    for (size_t i = 0; i < connectors.size(); i++) {
        connectors[i].notifyProfilesUpdated();
    }

    return found;
}

std::unique_ptr<ChargingSchedule> SmartChargingService::getCompositeSchedule(unsigned int connectorId, int duration, ChargingRateUnitType_Optional unit) {

    if (connectorId > 0 && !getScConnectorById(connectorId)) {
        MO_DBG_ERR("invalid args");
        return nullptr;
    }
    
    if (unit == ChargingRateUnitType_Optional::None) {
        if (powerSupported && !currentSupported) {
            unit = ChargingRateUnitType_Optional::Watt;
        } else if (!powerSupported && currentSupported) {
            unit = ChargingRateUnitType_Optional::Amp;
        }
    }

    if (connectorId > 0) {
        return getScConnectorById(connectorId)->getCompositeSchedule(duration, unit);
    }
    
    auto& startSchedule = context.getModel().getClock().now();

    auto schedule = std::unique_ptr<ChargingSchedule>(new ChargingSchedule());
    schedule->duration = duration;
    schedule->startSchedule = startSchedule;
    schedule->chargingProfileKind = ChargingProfileKindType::Absolute;
    schedule->recurrencyKind = RecurrencyKindType::NOT_SET;

    auto& periods = schedule->chargingSchedulePeriod;

    Timestamp periodBegin = Timestamp(startSchedule);
    Timestamp periodStop = Timestamp(startSchedule);

    while (periodBegin - startSchedule < duration && periods.size() < CHARGINGSCHEDULEMAXPERIODS) {

        //calculate limit
        ChargeRate limit;
        calculateLimit(periodBegin, limit, periodStop);

        //if the unit is still unspecified, guess by taking the unit of the first limit
        if (unit == ChargingRateUnitType_Optional::None) {
            if (limit.power < limit.current) {
                unit = ChargingRateUnitType_Optional::Watt;
            } else {
                unit = ChargingRateUnitType_Optional::Amp;
            }
        }

        periods.push_back(ChargingSchedulePeriod());
        float limit_opt = unit == ChargingRateUnitType_Optional::Watt ? limit.power : limit.current;
        periods.back().limit = limit_opt != std::numeric_limits<float>::max() ? limit_opt : -1.f;
        periods.back().numberPhases = limit.nphases != std::numeric_limits<int>::max() ? limit.nphases : -1;
        periods.back().startPeriod = periodBegin - startSchedule;

        periodBegin = periodStop;
    }

    if (unit == ChargingRateUnitType_Optional::Watt) {
        schedule->chargingRateUnit = ChargingRateUnitType::Watt;
    } else {
        schedule->chargingRateUnit = ChargingRateUnitType::Amp;
    }

    return schedule;
}

bool SmartChargingServiceUtils::printProfileFileName(char *out, size_t bufsize, unsigned int connectorId, ChargingProfilePurposeType purpose, unsigned int stackLevel) {
    int pret = 0;

    switch (purpose) {
        case (ChargingProfilePurposeType::ChargePointMaxProfile):
            pret = snprintf(out, bufsize, MO_FILENAME_PREFIX "sc-cm-%u.jsn", stackLevel);
            break;
        case (ChargingProfilePurposeType::TxDefaultProfile):
            pret = snprintf(out, bufsize, MO_FILENAME_PREFIX "sc-td-%u-%u.jsn", connectorId, stackLevel);
            break;
        case (ChargingProfilePurposeType::TxProfile):
            pret = snprintf(out, bufsize, MO_FILENAME_PREFIX "sc-tx-%u-%u.jsn", connectorId, stackLevel);
            break;
    }

    if (pret < 0 || (size_t) pret >= bufsize) {
        MO_DBG_ERR("fn error: %i", pret);
        return false;
    }

    return true;
}

bool SmartChargingServiceUtils::storeProfile(std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId, ChargingProfile *chargingProfile) {

    if (!filesystem) {
        return false;
    }

    DynamicJsonDocument chargingProfileJson {0};
    if (!chargingProfile->toJson(chargingProfileJson)) {
        return false;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};

    if (!printProfileFileName(fn, MO_MAX_PATH_SIZE, connectorId, chargingProfile->getChargingProfilePurpose(), chargingProfile->getStackLevel())) {
        return false;
    }

    return FilesystemUtils::storeJson(filesystem, fn, chargingProfileJson);
}

bool SmartChargingServiceUtils::removeProfile(std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId, ChargingProfilePurposeType purpose, unsigned int stackLevel) {

    if (!filesystem) {
        return false;
    }

    char fn [MO_MAX_PATH_SIZE] = {'\0'};

    if (!printProfileFileName(fn, MO_MAX_PATH_SIZE, connectorId, purpose, stackLevel)) {
        return false;
    }

    return filesystem->remove(fn);
}


