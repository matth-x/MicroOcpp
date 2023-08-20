// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SMARTCHARGINGSERVICE_H
#define SMARTCHARGINGSERVICE_H

#include <functional>
#include <array>

#include <ArduinoJson.h>

#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {

enum class ChargingRateUnitType_Optional {
    Watt,
    Amp,
    None
};

class Context;
class Model;

using ProfileStack = std::array<std::unique_ptr<ChargingProfile>, CHARGEPROFILEMAXSTACKLEVEL + 1>;

class SmartChargingConnector {
private:
    Model& model;
    std::shared_ptr<FilesystemAdapter> filesystem;
    const unsigned int connectorId;

    ProfileStack& ChargePointMaxProfile;
    ProfileStack& ChargePointTxDefaultProfile;
    ProfileStack TxDefaultProfile;
    ProfileStack TxProfile;

    std::function<void(float,float,int)> limitOutput;

    int trackTxRmtProfileId = -1; //optional Charging Profile ID when tx is started via RemoteStartTx
    Timestamp trackTxStart = MAX_TIME; //time basis for relative profiles
    int trackTxId = -1; //transactionId assigned by OCPP server

    Timestamp nextChange = MIN_TIME;

    ChargeRate trackLimitOutput;

    void calculateLimit(const Timestamp &t, ChargeRate& limitOut, Timestamp& validToOut);

    void trackTransaction();

public:
    SmartChargingConnector(Model& model, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId, ProfileStack& ChargePointMaxProfile, ProfileStack& ChargePointTxDefaultProfile);
    SmartChargingConnector(SmartChargingConnector&&) = default;
    ~SmartChargingConnector();

    void loop();

    void setSmartChargingOutput(std::function<void(float,float,int)> limitOutput); //read maximum Watt x Amps x numberPhases

    ChargingProfile *updateProfiles(std::unique_ptr<ChargingProfile> chargingProfile);

    void notifyProfilesUpdated();

    bool clearChargingProfile(std::function<bool(int, int, ChargingProfilePurposeType, int)> filter);

    std::unique_ptr<ChargingSchedule> getCompositeSchedule(int duration, ChargingRateUnitType_Optional unit);

    size_t getChargingProfilesCount();
};

class SmartChargingService {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;
    std::vector<SmartChargingConnector> connectors; //connectorId 0 excluded
    SmartChargingConnector *getScConnectorById(unsigned int connectorId);
    unsigned int numConnectors; //connectorId 0 included
    
    ProfileStack ChargePointMaxProfile;
    ProfileStack ChargePointTxDefaultProfile;

    std::function<void(float,float,int)> limitOutput;
    ChargeRate trackLimitOutput;
    bool powerSupported = false;
    bool currentSupported = false;

    Timestamp nextChange = MIN_TIME;

    ChargingProfile *updateProfiles(unsigned int connectorId, std::unique_ptr<ChargingProfile> chargingProfile);
    bool loadProfiles();

    void calculateLimit(const Timestamp &t, ChargeRate& limitOut, Timestamp& validToOut);
  
public:
    SmartChargingService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int numConnectors);
    ~SmartChargingService();

    void loop();

    void setSmartChargingOutput(unsigned int connectorId, std::function<void(float,float,int)> limitOutput); //read maximum Watt x Amps x numberPhases
    void updateAllowedChargingRateUnit(bool powerSupported, bool currentSupported); //set supported measurand of SmartChargingOutput

    bool setChargingProfile(unsigned int connectorId, std::unique_ptr<ChargingProfile> chargingProfile);

    bool clearChargingProfile(std::function<bool(int, int, ChargingProfilePurposeType, int)> filter);

    std::unique_ptr<ChargingSchedule> getCompositeSchedule(unsigned int connectorId, int duration, ChargingRateUnitType_Optional unit = ChargingRateUnitType_Optional::None);
};

//filesystem-related helper functions
namespace SmartChargingServiceUtils {
bool printProfileFileName(char *out, size_t bufsize, unsigned int connectorId, ChargingProfilePurposeType purpose, unsigned int stackLevel);
bool storeProfile(std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId, ChargingProfile *chargingProfile);
bool removeProfile(std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId, ChargingProfilePurposeType purpose, unsigned int stackLevel);
}

} //end namespace MicroOcpp

#endif
