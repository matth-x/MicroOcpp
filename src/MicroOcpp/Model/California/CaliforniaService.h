// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef CALIFORNIA_SERVICE_H
#define CALIFORNIA_SERVICE_H


#if MO_ENABLE_CALIFORNIA
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Version.h>

#include <cstdint>

namespace MicroOcpp {
class Context;

#if MO_ENABLE_V16
namespace v16 {

class MO_Connection;
class CaliforniaService;
class ConfigurationService;
class Configuration;
class TransactionServiceEvse;
class Transaction;


struct CaliforniaPrice
{
    bool           isValid;
    Timestamp      atTime;

    bool           isAtTimeValid;
    bool           isPricekWhPriceValid;
    bool           isPriceHourPriceValid;
    bool           isPriceFlatFeeValid;
    bool           isIdleGraceMinutesValid; // only be used in current stage price
    bool           isIdleHourPriceValid;
    double         chargingPricekWhPrice = 0.0;
    double         chargingPriceHourPrice = 0.0;
    double         chargingPriceFlatFee = 0.0;

    unsigned int   idleGraceMinutes = 0;
    double         idleHourPrice = 0.0;


    void reset();
};

struct CaliforniaTrigger
{
    bool                    isValid;

    bool                    isAtTimeValid;
    bool                    isAtEnergykWhValid;
    bool                    isAtPowerkWValid;
    bool                    isAtCPStatusValid;

    Timestamp               atTime;
    int32_t                 atEnergykWh;
    double                  atPowerkW;
    MO_ChargePointStatus    atCPStatus[6];

    void reset();
};

class CaliforniaServiceEvse : public MemoryManaged {
private:
    Context& context;
    CaliforniaService &californiaService;
    MO_Connection *connection = nullptr;
    TransactionServiceEvse *transactionServiceEvse = nullptr;
    MeteringServiceEvse *meteringServiceEvse = nullptr;
    AvailabilityServiceEvse *availabilityServiceEvse = nullptr;
    MO_FilesystemAdapter *filesystem = nullptr;

    ConfigurationService *configService = nullptr;

    unsigned int evseId;
    bool bIsSetUserPrice = false;

    // price and cost data
    Configuration* customDisplayCostAndPrice = nullptr;
    Configuration* defaultPrice = nullptr;
    String         defaultPriceText;
    String         priceText;
    String         priceTextOffline;

    CaliforniaPrice         price;
    CaliforniaPrice         nextPrice;
    CaliforniaTrigger       trigger;


    Transaction*            lastTransaction = nullptr;
    MO_ChargePointStatus    lastChargePointStatus = MO_ChargePointStatus::MO_ChargePointStatus_UNDEFINED;
    double                  lastPowerkW = 0.0;
    bool                    isCharging = false;
    double                  cost = 0.0; // total cost
    int32_t                 lastEnergykWh = 0; // energy consumption in kWh
    Timestamp               lastHourTime; // as unix time stamp
    unsigned int            lastDefaultPriceWriteCounter = 0;

    // multi-language and display data
    Configuration* customMultiLanguageMessages = nullptr;
    Configuration* supportedLanguages = nullptr;
    Configuration* timeOffset = nullptr;
    Configuration* nextTimeOffsetTransitionDateTime = nullptr;
    Configuration* timeOffsetNextTransition = nullptr;

    unsigned int   lastSupportedLanguagesWriteCounter = 0;
    unsigned int   lastTimeOffsetWriteCounter = 0;


    void (*priceTextOutput)(unsigned int evseId, const char* priceText, void *userData) = nullptr;
    void  *priceTextOutputUserData = nullptr;
    void (*priceTextOfflineOutput)(unsigned int evseId, const char* priceTextOffline, void *userData) = nullptr;
    void  *priceTextOfflineOutputUserData = nullptr;

    void (*kWhPriceOutput)(unsigned int evseId, float kWhPrice, void *userData) = nullptr;
    void  *kWhPriceOutputUserData = nullptr;
    void (*hourPriceOutput)(unsigned int evseId, float hourPrice, void *userData) = nullptr;
    void  *hourPriceOutputUserData = nullptr;
    void (*flatFreeOutput)(unsigned int evseId, float flatFree, void *userData) = nullptr;
    void  *flatFreeOutputUserData = nullptr;
    void (*chargingStatusOutput)(unsigned int evseId, bool isCharging, void *userData) = nullptr;
    void  *chargingStatusOutputUserData = nullptr;
    void (*costOutput)(unsigned int evseId, float cost, void *userData) = nullptr;
    void  *costOutputUserData = nullptr;

    void (*formatOutput)(unsigned int evseId, const char* format, void *userData) = nullptr;
    void  *formatOutputUserData = nullptr;
    void (*languageOutput)(unsigned int evseId, const char* language, void *userData) = nullptr;
    void  *languageOutputUserData = nullptr;
    void (*contentOutput)(unsigned int evseId, const char* format, const char *language, const char* content, void *userData) = nullptr;
    void  *contentOutputUserData = nullptr;
    void (*qrCodeTextOutput)(unsigned int evseId, const char* qrCodeText, void *userData) = nullptr;
    void  *qrCodeTextOutputUserData = nullptr;

    bool (*connectorPluggedInput)(unsigned int evseId) = nullptr;
    void  *connectorPluggedInputUserData = nullptr;
    void (*idleNotificationOutput)(unsigned int evseId, bool isIdleFee, void *userData) = nullptr;
    void  *idleNotificationOutputUserData = nullptr;
    void (*idleGraceMinutesOutput)(unsigned int evseId, int idleGraceMinutes, void *userData) = nullptr;
    void  *idleGraceMinutesOutputUserData = nullptr;

    void (*supportedLanguagesOutput)(unsigned int evseId, const char* supportedLanguages,  void *userData) = nullptr;
    void  *supportedLanguagesOutputUserData = nullptr;

    void (*timeOffsetOutput)(unsigned int evseId, const char* timeOffset, void *userData) = nullptr;
    void  *timeOffsetOutputUserData = nullptr;
public:
    CaliforniaServiceEvse(Context& context, CaliforniaService& service, unsigned int connectorId);

    bool setup();

    void loop();

    // Default Price - priceText and priceTextOffline
    // Runing Cost   - priceText
    // Final Cost    - priceText
    void setPriceTextOutput(void(*getString)(unsigned int, const char*, void*), void *userData);
    void setPriceTextOfflineOutput(void(*getString)(unsigned int, const char*, void*), void *userData);

    // Runing Cost - chargingPrice
    void setkWhPriceOutput(void(*getFloat)(unsigned int, float, void*), void *userData);
    void setHourPriceOutput(void(*getFloat)(unsigned int, float, void*), void *userData);
    void setFlatFeeOutput(void(*getFloat)(unsigned int, float, void*), void *userData);
    void setChargingStatusOutput(void(*getString)(unsigned int, bool, void*), void *userData);
    // Runing Cost - realtime cost
    // Final Cost  - cost
    void setCostOutput(void(*getFloat)(unsigned int, float, void*), void *userData);

    // Final Cost - priceTextExtra
    void setFormatOutput(void(*getString)(unsigned int, const char*, void*), void *userData);
    void setLanguageOutput(void(*getString)(unsigned int, const char*, void*), void *userData);
    void setContentOutput(void(*getString)(unsigned int, const char*, const char*, const char*, void*), void *userData);
    void setQrCodeTextOutput(void(*getString)(unsigned int, const char*,void*), void *userData);

    // Idle Fees
    void setConnectorPluggedInput(bool(*connectorPlugged)(unsigned int));
    void setIdleNotificationOutput(void(*getBool)(unsigned int, bool, void*), void *userData);
    void setIdleGraceMinutesOutput(void(*getInt)(unsigned int, int, void*), void *userData);

    // Multi-language
    void setSupportedLanguagesOutput(void(*getString)(unsigned int, const char*, void*), void *userData);

    // Timezone
    void setTimeOffsetOutput(void(*getString)(unsigned int, const char*, void*), void *userData);


    void updateUserPriceText(const char* priceText);
    void updatePrice(const CaliforniaPrice& price);
    void updateNextPrice(const CaliforniaPrice& price);
    void updateChargingStatus(bool isCharging);
    void updateCost(const double cost, unsigned int meterValue = 0);
    void updateTrigger(const CaliforniaTrigger& trigger);
    void updatePriceTextExtra(const char* priceTextExtra);
    void updateQrCodeText(const char* qrCodeText);
private:
    void reloadDefaultPriceText();
    void storeDefaultPriceJSON();
};

class CaliforniaService : public MemoryManaged {
private:
    Context& context;
    CaliforniaServiceEvse *evses[MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    Configuration* customDisplayCostAndPrice = nullptr;
public:
    CaliforniaService(Context& context);

    bool setup();

    CaliforniaServiceEvse *getEvse(unsigned int evseId);

    bool getCustomDisplayCostAndPrice();

    void loop();
};
} //namespace v16
#endif //MO_ENABLE_V16
} //namespace MicroOcpp

#if MO_ENABLE_V201
namespace MicroOcpp {

class Context;

namespace v201 {
class ConfigurationService;
class Configuration;
class TransactionService;

class CaliforniaService : public MemoryManaged {
private:
    Context& context;

    int ocppVersion = -1;

    MO_FilesystemAdapter *filesystem = nullptr;

    ConfigurationService *configService = nullptr;
    Configuration *customDisplayCostAndPrice = nullptr;
    TransactionService *transactionService = nullptr;

public:
    CaliforniaService(Context& context);

    bool setup();

    void loop();
};
} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201

#endif //MO_ENABLE_CALIFORNIA
#endif //CALIFORNIA_SERVICE_H
