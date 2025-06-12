// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SMARTCHARGINGSERVICE_H
#define MO_SMARTCHARGINGSERVICE_H

#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

#define MO_CHARGEPROFILESTACK_SIZE (MO_ChargeProfileMaxStackLevel + 1)

namespace MicroOcpp {

class Context;

class SmartChargingService;

#if MO_ENABLE_V201
namespace Ocpp201 {
class Variable;
}
#endif //MO_ENABLE_V201

class SmartChargingServiceEvse : public MemoryManaged {
private:
    Context& context;
    Clock& clock;
    SmartChargingService& scService;
    const unsigned int evseId;

    ChargingProfile *txDefaultProfile [MO_CHARGEPROFILESTACK_SIZE] = {nullptr};
    ChargingProfile *txProfile [MO_CHARGEPROFILESTACK_SIZE] = {nullptr};

    void (*limitOutput)(MO_ChargeRate limit, unsigned int evseId, void *userData) = nullptr;
    void *limitOutputUserData = nullptr;
    MO_ChargeRate trackLimitOutput;

    int trackTxRmtProfileId = -1; //optional Charging Profile ID when tx is started via RemoteStartTx
    Timestamp trackTxStart; //time basis for relative profiles
    #if MO_ENABLE_V16
    int trackTransactionId16 = -1; //transactionId
    #endif
    #if MO_ENABLE_V201
    char trackTransactionId201[MO_TXID_SIZE] = {'\0'}; //transactionId
    #endif //MO_ENABLE_V201

    Timestamp nextChange;

    void calculateLimit(int32_t unixTime, int32_t sessionDurationSecs, MO_ChargeRate& limit, int32_t& nextChangeSecs);

    void trackTransaction();

public:
    SmartChargingServiceEvse(Context& context, SmartChargingService& scService, unsigned int evseId);
    SmartChargingServiceEvse(SmartChargingServiceEvse&&) = default;
    ~SmartChargingServiceEvse();

    void loop();

    void setSmartChargingOutput(void (*limitOutput)(MO_ChargeRate limit, unsigned int evseId, void *userData), bool powerSupported, bool currentSupported, bool phases3to1Supported, bool phaseSwitchingSupported, void *userData);

    bool updateProfile(std::unique_ptr<ChargingProfile> chargingProfile, bool updateFile);

    void notifyProfilesUpdated();

    bool clearChargingProfile(int chargingProfileId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel);

    std::unique_ptr<ChargingSchedule> getCompositeSchedule(int duration, ChargingRateUnitType unit);

    size_t getChargingProfilesCount();
};

class SmartChargingService : public MemoryManaged {
private:
    Context& context;
    Clock& clock;
    MO_FilesystemAdapter *filesystem = nullptr;
    SmartChargingServiceEvse* evses [MO_NUM_EVSEID] = {nullptr}; //evseId 0 won't be populated
    unsigned int numEvseId = MO_NUM_EVSEID;

    ChargingProfile *chargePointMaxProfile [MO_CHARGEPROFILESTACK_SIZE] = {nullptr};
    ChargingProfile *chargePointTxDefaultProfile [MO_CHARGEPROFILESTACK_SIZE] = {nullptr};

    void (*limitOutput)(MO_ChargeRate limit, unsigned int evseId, void *userData) = nullptr;
    void *limitOutputUserData = nullptr;
    bool powerSupported = false;
    bool currentSupported = false;
    bool phases3to1Supported = false;
    #if MO_ENABLE_V201
    bool phaseSwitchingSupported = false;
    #endif
    MO_ChargeRate trackLimitOutput;

    Timestamp nextChange;

    int ocppVersion = -1;

    #if MO_ENABLE_V201
    Ocpp201::Variable *chargingProfileEntriesInt201 = nullptr;
    #endif //MO_ENABLE_V201

    SmartChargingServiceEvse *getEvse(unsigned int evseId);
    bool updateProfile(unsigned int evseId, std::unique_ptr<ChargingProfile> chargingProfile, bool updateFile);
    bool loadProfiles();

    size_t getChargingProfilesCount();

    void calculateLimit(int32_t unixTime, MO_ChargeRate& limit, int32_t& nextChangeSecs);

public:
    SmartChargingService(Context& context);
    ~SmartChargingService();

    bool setSmartChargingOutput(unsigned int evseId, void (*limitOutput)(MO_ChargeRate limit, unsigned int evseId, void *userData), bool powerSupported, bool currentSupported, bool phases3to1Supported, bool phaseSwitchingSupported, void *userData);

    bool setup();

    void loop();

    bool setChargingProfile(unsigned int evseId, std::unique_ptr<ChargingProfile> chargingProfile);

    bool clearChargingProfile(int chargingProfileId, int evseId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel);

    std::unique_ptr<ChargingSchedule> getCompositeSchedule(unsigned int evseId, int duration, ChargingRateUnitType unit = ChargingRateUnitType::UNDEFINED);

friend class SmartChargingServiceEvse;
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
#endif
