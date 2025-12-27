// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/DataTransfer/DataTransferService.h>
#include <MicroOcpp/Model/California/CaliforniaService.h>
#include <MicroOcpp/Model/California/CaliforniaUtils.h>
#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Operations/DataTransferSetUserPrice.h>
#include <MicroOcpp/Operations/DataTransferRunningCost.h>
#include <MicroOcpp/Operations/DataTransferFinalCost.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>
#include <ArduinoJson.h>


#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA

using namespace MicroOcpp;


void v16::CaliforniaPrice::reset()
{
    isValid = false;

    isAtTimeValid = false;
    isPricekWhPriceValid = false;
    isPriceHourPriceValid = false;
    isPriceFlatFeeValid = false;
    isIdleGraceMinutesValid = false;
    isIdleHourPriceValid = false;
    chargingPricekWhPrice = 0.0;
    chargingPriceHourPrice = 0.0;
    chargingPriceFlatFee = 0.0;

    idleGraceMinutes = 0;
    idleHourPrice = 0.0;
}

void v16::CaliforniaTrigger::reset()
{
    isValid = false;
    isAtCPStatusValid = false;
    isAtEnergykWhValid = false;
    isAtPowerkWValid = false;
    isAtTimeValid = false;

    atEnergykWh = 0;
    atPowerkW = 0;
    for (int i = 0; i < 6; i++) {
        atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_UNDEFINED;
    }
}


v16::CaliforniaServiceEvse::CaliforniaServiceEvse(Context& context, CaliforniaService& service, unsigned int connectorId) :
    MemoryManaged("v16.California.CaliforniaServiceEvse"),
    context(context),
    californiaService(service),
    evseId(connectorId)
{
}

void v16::CaliforniaServiceEvse::reloadDefaultPriceText()
{
    // if reload default price text, means cache is invalid
    trigger.reset();
    nextPrice.reset();
    price.reset();


    if (defaultPrice->getString() == nullptr || strlen(defaultPrice->getString()) == 0) {
        defaultPriceText.clear();
        priceText.clear();
        priceTextOffline.clear();
        return;
    }
    
    size_t capacity = 512;

    auto doc = initJsonDoc("v16.California.DefaultPriceLoader", capacity);

    DeserializationError err = DeserializationError::NoMemory;

    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc = initJsonDoc("v16.California.DefaultPriceLoader", capacity);
        err = deserializeJson(doc, defaultPrice->getString(), strlen(defaultPrice->getString()));
        capacity *= 2;
    }

    switch (err.code()) {
        case DeserializationError::Ok: {
            auto defaultPriceObject = doc.as<JsonObject>();
            if (defaultPriceObject.containsKey("priceText"))
            {
                if (strcmp(priceText.c_str(), defaultPriceObject["priceText"].as<const char *>()) != 0)
                {
                    priceText = defaultPriceObject["priceText"].as<const char *>();
                    if (priceTextOutput)
                        priceTextOutput(evseId, priceText.c_str(), priceTextOutputUserData);
                }
            }
            else 
                MO_DBG_ERR("No priceText key in JSON");
            break;

            // optional offline price text
            if (defaultPriceObject.containsKey("priceTextOffline"))
            {
                if (strcmp(priceTextOffline.c_str(), defaultPriceObject["priceTextOffline"].as<const char *>()) != 0)
                {
                    priceTextOffline = defaultPriceObject["priceTextOffline"].as<const char *>();
                    if (priceTextOfflineOutput)
                        priceTextOfflineOutput(evseId, priceTextOffline.c_str(), priceTextOfflineOutputUserData);
                }
            }

            // DefaultPrice's chargingPrice only use to start a session while offline.
            if (!defaultPriceObject.containsKey("priceTextOffline") || priceTextOffline.empty())
            {
                // offline is disabled by default
                priceTextOffline.clear();
            }

            // optional charging price
            price.isValid = defaultPriceObject.containsKey("chargingPrice");
            if (price.isValid) {
                auto chargingPriceObject = defaultPriceObject["chargingPrice"].as<JsonObject>();
                price.isPricekWhPriceValid = chargingPriceObject.containsKey("kWhPrice");
                if (price.isPricekWhPriceValid)
                    price.chargingPricekWhPrice = chargingPriceObject["kWhPrice"].as<double>();
                if (kWhPriceOutput)
                    kWhPriceOutput(evseId, price.isPricekWhPriceValid ? price.chargingPricekWhPrice : 0, kWhPriceOutputUserData);

                price.isPriceHourPriceValid = chargingPriceObject.containsKey("hourPrice");
                if (price.isPriceHourPriceValid)
                    price.chargingPriceHourPrice = chargingPriceObject["hourPrice"].as<double>();
                if (hourPriceOutput)
                    hourPriceOutput(evseId, price.isPriceHourPriceValid ? price.chargingPriceHourPrice : 0, hourPriceOutputUserData);
                
                price.isPriceFlatFeeValid = chargingPriceObject.containsKey("flatFee");
                if (price.isPriceFlatFeeValid)
                    price.chargingPriceFlatFee = chargingPriceObject["flatFee"].as<double>();
                if (flatFreeOutput)
                    flatFreeOutput(evseId, price.isPriceFlatFeeValid ? price.chargingPriceFlatFee : 0, flatFreeOutputUserData);
            } else {
                if (kWhPriceOutput)
                    kWhPriceOutput(evseId, 0, kWhPriceOutputUserData);
                if (hourPriceOutput)
                    hourPriceOutput(evseId, 0, hourPriceOutputUserData);
                if (flatFreeOutput)
                    flatFreeOutput(evseId, 0, flatFreeOutputUserData);
            }
            break;
        }
        case DeserializationError::InvalidInput:
            MO_DBG_ERR("Invalid input! Not a JSON");
            break;
        case DeserializationError::NoMemory:
            MO_DBG_ERR("Not enough memory to store JSON document");
            break;
        default:
            MO_DBG_ERR("Deserialization failed: %s", err.c_str());
            break;
    }

    price.atTime = context.getClock().now();
}


bool v16::CaliforniaServiceEvse::setup()
{
    configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    transactionServiceEvse = context.getModel16().getTransactionService()->getEvse(evseId);
    if (!transactionServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    meteringServiceEvse = context.getModel16().getMeteringService()->getEvse(evseId);
    if (!meteringServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    availabilityServiceEvse = context.getModel16().getAvailabilityService()->getEvse(evseId);
    if (!availabilityServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }


    // price and cost properties
    customDisplayCostAndPrice = configService->declareConfiguration<bool>("CustomDisplayCostAndPrice", false);
    if (!customDisplayCostAndPrice) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("no FS access. Can enqueue only one transaction");
    }

    defaultPrice = configService->declareConfiguration<const char *>("DefaultPrice", "");
    if (!defaultPrice) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    configService->registerValidator("DefaultPrice", CaliforniaUtils::validateDefaultPrice, nullptr);

    // customDisplayCostAndPrice is enabled, then the default price configuration must be set
    if (customDisplayCostAndPrice->getBool())
        reloadDefaultPriceText();

    transactionServiceEvse = context.getModel16().getTransactionService()->getEvse(evseId);
    if (!transactionServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }


    // multi-language support
    customMultiLanguageMessages = configService->declareConfiguration<bool>("CustomMultiLanguageMessages", false);
    if (!customMultiLanguageMessages) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    supportedLanguages = configService->declareConfiguration<const char *>("SupportedLanguages", "en");
    if (!supportedLanguages) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    configService->registerValidator("SupportedLanguages", CaliforniaUtils::validateSupportLanguages, nullptr);


    timeOffset = configService->declareConfiguration<const char *>("TimeOffset", "+00:00");
    if (!timeOffset) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    configService->registerValidator("TimeOffset", CaliforniaUtils::validateTimeOffset, nullptr);

    nextTimeOffsetTransitionDateTime = configService->declareConfiguration<const char *>("NextTimeOffsetTransitionDateTime", "");
    if (!nextTimeOffsetTransitionDateTime) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    configService->registerValidator("NextTimeOffsetTransitionDateTime", CaliforniaUtils::validateNextTimeOffsetTransitionDateTime, &context);

    timeOffsetNextTransition = configService->declareConfiguration<const char *>("TimeOffsetNextTransition", "+00:00");
    if (!timeOffsetNextTransition) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    configService->registerValidator("TimeOffsetNextTransition", CaliforniaUtils::validateTimeOffset, nullptr);

    return true;
}

void v16::CaliforniaServiceEvse::loop()
{
    // configuration change
    // if transaction is authorized, any changes of default price would be disabled temporarily,
    // MO will use user specified price or running cost instead of default price
    if (!transactionServiceEvse->getTransaction())
    {
        lastTransaction = nullptr;
        // two case:
        // 1. lastWriteCounter is set to 0 when CaliforniaServiceEvse is created, so that reloadDefaultPriceText() will be called at the first loop()
        // 2. defaultPrice is changed while no transaction is running
        if (lastDefaultPriceWriteCounter != defaultPrice->getWriteCount())
        {
            lastDefaultPriceWriteCounter = defaultPrice->getWriteCount();
            reloadDefaultPriceText();
        }
    } else if (lastTransaction != transactionServiceEvse->getTransaction()) {
        // transaction was finished reload default price text
        lastTransaction = transactionServiceEvse->getTransaction();
        reloadDefaultPriceText();
    }


    // multi-language support
    if (customMultiLanguageMessages->getBool())
    {
        if (lastSupportedLanguagesWriteCounter != supportedLanguages->getWriteCount())
        {
            lastSupportedLanguagesWriteCounter = supportedLanguages->getWriteCount();
            if (supportedLanguagesOutput)
                supportedLanguagesOutput(evseId, supportedLanguages->getString(), supportedLanguagesOutputUserData);

        }
        if (lastTimeOffsetWriteCounter != timeOffset->getWriteCount())
        {
            lastTimeOffsetWriteCounter = timeOffset->getWriteCount();
            if (timeOffsetOutput)
                timeOffsetOutput(evseId, timeOffset->getString(), timeOffsetOutputUserData);
        }
    }

    // timezone transition support
    // '\0' occupies one character
    if (strlen(nextTimeOffsetTransitionDateTime->getString()) > 1)
    {
        int32_t delta;
        Timestamp transitionTime;
        context.getClock().parseString(nextTimeOffsetTransitionDateTime->getString(), transitionTime);
        if (transitionTime.isDefined())
        {
            context.getClock().delta(context.getClock().now(), transitionTime, delta);
            if (delta <= 0)
            {
                // transition time is reached, update time offset
                if (timeOffsetOutput)
                    timeOffsetOutput(evseId, timeOffsetNextTransition->getString(), timeOffsetOutputUserData);
                timeOffset->setString(nextTimeOffsetTransitionDateTime->getString());
                nextTimeOffsetTransitionDateTime->setString(""); // clear transition time after transition
            }
        }
        else {
            nextTimeOffsetTransitionDateTime->setString(""); // clear invalid transition time
            MO_DBG_ERR("invalid transition time format");
        }
    }


    // if not transcation isn't running, our operations are finished
    if (transactionServiceEvse->getTransaction() == nullptr)
        return;


    // trigger meterValues
    int32_t energyWh = meteringServiceEvse->readTxEnergyMeter(MO_ReadingContext::MO_ReadingContext_Other);
    if (trigger.isValid)
    {
        if (trigger.isAtCPStatusValid)
        {
            for (auto atPStatus : trigger.atCPStatus)
            {
                if (atPStatus == MO_ChargePointStatus::MO_ChargePointStatus_UNDEFINED)
                    break;

                if (atPStatus == lastChargePointStatus)
                    break;

                if (atPStatus == availabilityServiceEvse->getStatus())
                {
                    lastChargePointStatus = atPStatus;
                    meteringServiceEvse->triggerMeterValues();
                }
            }
        }
        if (trigger.isAtEnergykWhValid)
        {
            if (trigger.atEnergykWh >= energyWh)
            {
                meteringServiceEvse->triggerMeterValues();
                trigger.isAtEnergykWhValid = false; // conditon only reached once
            }
        }

        // TODO: get realtime power and check whether to trigger meterValues
        // if (trigger.isAtPowerkWValid)
        // {
        //     if (std::fabs(lastPowerKW - powerKW) >= trigger.atPowerKW)
        //         meteringServiceEvse->triggerMeterValues();
        // }

        if (trigger.isAtTimeValid)
        {
            int32_t delta;
            if (context.getClock().deltaMs(context.getClock().now(), trigger.atTime, delta) && delta <= 0)
            {
                meteringServiceEvse->triggerMeterValues();
                trigger.isAtTimeValid = false; // conditon only reached once
            }
        }
    } // if (trigger.isValid)



    // check whether need to update price category
    if (nextPrice.isValid)
    {
        int32_t delta;
        context.getClock().delta(context.getClock().now(), nextPrice.atTime, delta);

        price.isValid = true;

        // update price category from nextPrice
        price.isPricekWhPriceValid  = nextPrice.isPricekWhPriceValid;
        if (nextPrice.isPricekWhPriceValid && nextPrice.chargingPricekWhPrice != price.chargingPricekWhPrice)
        {
            price.chargingPricekWhPrice = nextPrice.chargingPricekWhPrice;
            if (kWhPriceOutput)
                kWhPriceOutput(evseId, price.chargingPricekWhPrice, kWhPriceOutputUserData);
        }

        price.isPriceHourPriceValid = nextPrice.isPriceHourPriceValid;
        if (nextPrice.isPriceHourPriceValid && nextPrice.chargingPriceHourPrice != price.chargingPriceHourPrice)
        {
            price.chargingPriceHourPrice = nextPrice.chargingPriceHourPrice;
            if (hourPriceOutput)
                hourPriceOutput(evseId, price.chargingPriceHourPrice, hourPriceOutputUserData);
        }

        price.isPriceFlatFeeValid  = nextPrice.isPriceFlatFeeValid;
        if (nextPrice.isPriceFlatFeeValid && nextPrice.chargingPriceFlatFee != price.chargingPriceFlatFee)
        {
            price.chargingPriceFlatFee = nextPrice.chargingPriceFlatFee;
            if (flatFreeOutput)
                flatFreeOutput(evseId, price.chargingPriceFlatFee, flatFreeOutputUserData);
        }

        price.isIdleGraceMinutesValid = nextPrice.isIdleGraceMinutesValid;
        if (nextPrice.isIdleGraceMinutesValid && nextPrice.idleGraceMinutes != price.idleGraceMinutes)
        {
            price.idleGraceMinutes = nextPrice.idleGraceMinutes;
            if (idleGraceMinutesOutput)
                idleGraceMinutesOutput(evseId, price.idleGraceMinutes, idleGraceMinutesOutputUserData);
        }

        price.isIdleHourPriceValid   = nextPrice.isIdleHourPriceValid;
        if (nextPrice.isIdleHourPriceValid && nextPrice.idleHourPrice != price.idleHourPrice)
        {
            price.idleHourPrice = nextPrice.idleHourPrice;
            if (idleNotificationOutput)
                idleNotificationOutput(evseId, price.idleHourPrice > 0.0, idleNotificationOutputUserData);
        }

        nextPrice.isValid = false; // only update once
    } // if (nextPrice.isValid)
    


    // calculate cost during transaction
    if (price.isValid && lastTransaction->isRunning())
    {
        if (price.isPricekWhPriceValid)
        {
            cost += (energyWh - lastEnergykWh) / 1000.0 * price.chargingPricekWhPrice;
            lastEnergykWh = energyWh;
        }

        // add hour price cost if exists
        if (price.isPriceHourPriceValid)
        {
            int32_t delta;
            Timestamp hourTime = context.getClock().now();
            context.getClock().delta(hourTime, lastHourTime, delta);
            cost += (delta / 3600.0) * price.chargingPriceHourPrice;
            lastHourTime = hourTime;
        }
        
        // add flat fee if exists
        if (price.isPriceFlatFeeValid)
            cost += price.chargingPriceFlatFee;

        if (costOutput)
            costOutput(evseId, cost, costOutputUserData);
    } // if (price.isValid && transactionServiceEvse->getTransaction()->isRunning())
}

void v16::CaliforniaServiceEvse::setPriceTextOutput(void(*getString)(unsigned int, const char*, void*), void *userData)
{
    priceTextOutput = getString;
    priceTextOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setPriceTextOfflineOutput(void(*getString)(unsigned int, const char*, void*), void *userData)
{
    priceTextOfflineOutput = getString;
    priceTextOfflineOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setkWhPriceOutput(void(*getFloat)(unsigned int, float, void*), void *userData)
{
    kWhPriceOutput = getFloat;
    kWhPriceOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setHourPriceOutput(void(*getFloat)(unsigned int, float, void*), void *userData)
{
    hourPriceOutput = getFloat;
    hourPriceOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setFlatFeeOutput(void(*getFloat)(unsigned int, float, void*), void *userData)
{
    flatFreeOutput = getFloat;
    flatFreeOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setChargingStatusOutput(void(*getString)(unsigned int, bool, void*), void *userData)
{
    chargingStatusOutput = getString;
    chargingStatusOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setCostOutput(void(*getFloat)(unsigned int, float, void*), void *userData)
{
    costOutput = getFloat;
    costOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setFormatOutput(void(*getString)(unsigned int, const char*, void*), void *userData)
{
    formatOutput = getString;
    formatOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setLanguageOutput(void(*getString)(unsigned int, const char*, void*), void *userData)
{
    languageOutput = getString;
    languageOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setContentOutput(void(*getString)(unsigned int, const char*, const char*, const char*, void*), void *userData)
{
    contentOutput = getString;
    contentOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setQrCodeTextOutput(void(*getString)(unsigned int, const char*,void*), void *userData)
{
    qrCodeTextOutput = getString;
    qrCodeTextOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int))
{
    connectorPluggedInput = connectorPlugged;
}

void v16::CaliforniaServiceEvse::setIdleNotificationOutput(void(*getBool)(unsigned int, bool, void*), void *userData)
{
    idleNotificationOutput = getBool;
    idleNotificationOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setIdleGraceMinutesOutput(void(*getInt)(unsigned int, int, void*), void *userData)
{
    idleGraceMinutesOutput = getInt;
    idleGraceMinutesOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setSupportedLanguagesOutput(void(*getString)(unsigned int, const char*, void*), void *userData)
{
    supportedLanguagesOutput = getString;
    supportedLanguagesOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::setTimeOffsetOutput(void(*getString)(unsigned int, const char*, void*), void *userData)
{
    timeOffsetOutput = getString;
    timeOffsetOutputUserData = userData;
}

void v16::CaliforniaServiceEvse::updateUserPriceText(const char* priceText)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isAuthorized())
    {
        // only after transaction is authorized, CSMS can send SetUserPrice
        this->priceText = priceText;
        if (priceTextOutput && customDisplayCostAndPrice->getBool())
            priceTextOutput(evseId, priceText, priceTextOutputUserData);
    }
}

void v16::CaliforniaServiceEvse::updatePrice(const CaliforniaPrice& price)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isRunning())
    {
        this->price = price;
        if (customDisplayCostAndPrice->getBool())
        {
            if (kWhPriceOutput)
                kWhPriceOutput(evseId, price.isPricekWhPriceValid ? price.chargingPricekWhPrice : 0, kWhPriceOutputUserData);
            if (hourPriceOutput)
                hourPriceOutput(evseId, price.isPriceHourPriceValid ? price.chargingPriceHourPrice : 0, hourPriceOutputUserData);
            if (flatFreeOutput)
                flatFreeOutput(evseId, price.isPriceFlatFeeValid ? price.chargingPriceFlatFee : 0, flatFreeOutputUserData);
        }
    }
}

void v16::CaliforniaServiceEvse::updateNextPrice(const CaliforniaPrice& price)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isRunning())
    {
        this->nextPrice = price;
    }
}

void v16::CaliforniaServiceEvse::updateChargingStatus(bool isCharging)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isRunning())
    {
        if (this->isCharging != isCharging)
        {
            this->isCharging = isCharging;
            if (chargingStatusOutput && customDisplayCostAndPrice->getBool())
                chargingStatusOutput(evseId, isCharging, chargingStatusOutputUserData);
        }
    }
}

void v16::CaliforniaServiceEvse::updateCost(const double cost, unsigned int meterValue)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isRunning())
    {
        // discard meterValue now, because in ocppv201, only cost is being sent
        (void)meterValue;
        this->cost = cost;
        if (costOutput && customDisplayCostAndPrice->getBool())
            costOutput(evseId, cost, costOutputUserData);
    }
}

void v16::CaliforniaServiceEvse::updateTrigger(const CaliforniaTrigger& trigger)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isRunning())
    {
        this->trigger = trigger;
    }
}

void v16::CaliforniaServiceEvse::updatePriceTextExtra(const char* priceTextExtra)
{
    size_t capacity = 512;
    auto doc = initJsonDoc("v16.CaliforniaServiceEvse.priceTextExtra", capacity);
    DeserializationError err = DeserializationError::NoMemory;
    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {
        doc = initJsonDoc("v16.CaliforniaServiceEvse.priceTextExtra", capacity);
        err = deserializeJson(doc, priceTextExtra, strlen(priceTextExtra));
        capacity *= 2;
    }
    if (err != DeserializationError::Ok) {
        MO_DBG_DEBUG("priceTextExtra JSON deserialization failed: %s", err.c_str());
        return;
    }

    auto priceTextExtraArray = doc.as<JsonArray>();
    for (JsonVariant v : priceTextExtraArray) {
        if (v.is<JsonObject>()) {
            auto obj = v.as<JsonObject>();
            if (!obj.containsKey("format") || !obj.containsKey("language") || !obj.containsKey("content")) {
                MO_DBG_DEBUG("priceTextExtra JSON object missing required keys");
                continue;
            }
            const char* format = obj["format"];
            const char* language = obj["language"];
            const char* content = obj["content"];
            if (supportedLanguages->getBool() && !strstr(supportedLanguages->getString(), language))
            {
                MO_DBG_DEBUG("language %s not supported", language);
                continue;
            }
            else 
            {
                if (contentOutput)
                    contentOutput(evseId, format, language, content, contentOutputUserData);
            }
        }
    }
}

void v16::CaliforniaServiceEvse::updateQrCodeText(const char* qrCodeText)
{
    if (transactionServiceEvse->getTransaction() != nullptr && transactionServiceEvse->getTransaction()->isRunning())
    {
        if (qrCodeTextOutput && customDisplayCostAndPrice->getBool())
            qrCodeTextOutput(evseId, qrCodeText, qrCodeTextOutputUserData);
    }
}

v16::CaliforniaService::CaliforniaService(Context& context) : 
    MemoryManaged("v16.California.CaliforniaService"),
    context(context)
{
}

v16::CaliforniaServiceEvse *v16::CaliforniaService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new CaliforniaServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

bool v16::CaliforniaService::setup()
{
    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    customDisplayCostAndPrice = configService->declareConfiguration<bool>("CustomDisplayCostAndPrice", false);
    if (!customDisplayCostAndPrice) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    numEvseId = context.getModel16().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("connector init failure");
            return false;
        }
    }

    auto dataTransferService = context.getModel().getDataTransferService();
    if (!dataTransferService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    // register data transfer operations
    dataTransferService->registerOperation("org.openchargealliance.costmsg", "SetUserPrice", [](Context& context) -> Operation* {
        return new v16::DataTransferSetUserPrice(context.getModel16()); });
    dataTransferService->registerOperation("org.openchargealliance.costmsg", "FinalCost", [](Context& context) -> Operation* {
        return new v16::DataTransferFinalCost(context); });
    dataTransferService->registerOperation("org.openchargealliance.costmsg", "RunningCost", [](Context& context) -> Operation* {
        return new v16::DataTransferRunningCost(context); });

    return true;
}

bool v16::CaliforniaService::getCustomDisplayCostAndPrice() {
    if (customDisplayCostAndPrice) {
        return customDisplayCostAndPrice->getBool();
    }
    return false;
}

void v16::CaliforniaService::loop()
{
    if (!customDisplayCostAndPrice->getBool())
        return;

    numEvseId = context.getModel16().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (getEvse(i)) {
            getEvse(i)->loop();
        }
    }
}
#endif // MO_ENABLE_CALIFORNIA
#endif // MO_ENABLE_V16 || MO_ENABLE_V201