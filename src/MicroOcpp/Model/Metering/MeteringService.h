// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_METERINGSERVICE_H
#define MO_METERINGSERVICE_H

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

#ifndef MO_MVRECORD_SIZE
#define MO_MVRECORD_SIZE 10
#endif

namespace MicroOcpp {

class Context;
class Clock;
class Operation;
class Request;

namespace Ocpp16 {

class Model;
class MeteringService;
class Configuration;
class TransactionServiceEvse;
class Transaction;

class MeteringServiceEvse : public MemoryManaged, public RequestQueue {
private:
    Context& context;
    Clock& clock;
    Model& model;
    MeteringService& mService;
    const unsigned int connectorId;
    MO_FilesystemAdapter *filesystem = nullptr;
    TransactionServiceEvse *txSvcEvse = nullptr;

    Vector<MeterValue*> meterData; //has ownership
    MeterValue *meterDataFront = nullptr; //has ownership
    bool meterValuesInProgress = false;

    Vector<MO_MeterInput> meterInputs;
    int32_t (*txEnergyMeterInput)() = nullptr;
    int32_t (*txEnergyMeterInput2)(MO_ReadingContext readingContext, unsigned int evseId, void *user_data) = nullptr;
    void *txEnergyMeterInput2_user_data = nullptr;

    MeterValue *takeMeterValue(MO_ReadingContext readingContext, uint8_t inputSelectFlag);

    int32_t lastSampleTime = 0; //in secs since boot
    Timestamp lastAlignedTime; //as unix time stamp
    bool trackTxRunning = false;

    bool addTxMeterData(Transaction& transaction, MeterValue *mv);

    uint16_t trackSelectInputs = -1;
    void updateInputSelectFlags();

public:
    MeteringServiceEvse(Context& context, MeteringService& mService, unsigned int connectorId);
    virtual ~MeteringServiceEvse();

    bool addMeterInput(MO_MeterInput meterInput);
    Vector<MO_MeterInput>& getMeterInputs();

    /* Specify energy meter for Start- and StopTx (if not used, MO will select general-purpose energy meter)
     * `setTxEnergyMeterInput(cb)`: set simple callback without args
     * `setTxEnergyMeterInput2(cb)`: set complex callback. When MO executes the callback, it passes the ReadingContext (i.e. if measurement is
     * taken at the start, middle or stop of a transactinon), the evseId and the user_data pointer which the caller can freely select */
    bool setTxEnergyMeterInput(int32_t (*getInt)());
    bool setTxEnergyMeterInput2(int32_t (*getInt2)(MO_ReadingContext readingContext, unsigned int evseId, void *user_data), void *user_data = nullptr);

    bool setup();

    void loop();

    int32_t readTxEnergyMeter(MO_ReadingContext readingContext);

    Operation *createTriggeredMeterValues();

    bool beginTxMeterData(Transaction *transaction);

    bool endTxMeterData(Transaction *transaction);

    //RequestQueue implementation
    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;
};

class MeteringService : public MemoryManaged {
private:
    Context& context;
    Connection *connection = nullptr;

    MeteringServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    Configuration *meterValuesSampledDataString = nullptr;
    Configuration *stopTxnSampledDataString = nullptr;
    Configuration *meterValueSampleIntervalInt = nullptr;

    Configuration *meterValuesAlignedDataString = nullptr;
    Configuration *stopTxnAlignedDataString = nullptr;
    Configuration *clockAlignedDataIntervalInt = nullptr;

    Configuration *meterValuesInTxOnlyBool = nullptr;
    Configuration *stopTxnDataCapturePeriodicBool = nullptr;

    Configuration *transactionMessageAttemptsInt = nullptr;
    Configuration *transactionMessageRetryIntervalInt = nullptr;
public:
    MeteringService(Context& context);
    ~MeteringService();

    bool setup();

    void loop();

    MeteringServiceEvse *getEvse(unsigned int evseId);

friend class MeteringServiceEvse;
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

namespace MicroOcpp {

class Context;

namespace Ocpp201 {

class MeteringService;
class Variable;

class MeteringServiceEvse : public MemoryManaged {
private:
    Context& context;
    MeteringService& mService;
    const unsigned int evseId;
    
    Vector<MO_MeterInput> meterInputs;

    uint16_t trackSelectInputs = -1;
    void updateInputSelectFlags();
public:
    MeteringServiceEvse(Context& context, MeteringService& mService, unsigned int evseId);

    bool addMeterInput(MO_MeterInput meterInput);
    Vector<MO_MeterInput>& getMeterInputs();

    bool setup();

    std::unique_ptr<MeterValue> takeTxStartedMeterValue(MO_ReadingContext context = MO_ReadingContext_TransactionBegin);
    std::unique_ptr<MeterValue> takeTxUpdatedMeterValue(MO_ReadingContext context = MO_ReadingContext_SamplePeriodic);
    std::unique_ptr<MeterValue> takeTxEndedMeterValue(MO_ReadingContext context);
    std::unique_ptr<MeterValue> takeTriggeredMeterValues();
};

class MeteringService : public MemoryManaged {
private:
    Context& context;
    MeteringServiceEvse* evses [MO_NUM_EVSEID] = {nullptr};
    unsigned int numEvseId = MO_NUM_EVSEID;

    Variable *sampledDataTxStartedMeasurands = nullptr;
    Variable *sampledDataTxUpdatedMeasurands = nullptr;
    Variable *sampledDataTxEndedMeasurands = nullptr;
    Variable *alignedDataMeasurands = nullptr;
public:
    MeteringService(Context& context);
    ~MeteringService();

    MeteringServiceEvse *getEvse(unsigned int evseId);

    bool setup();

friend class MeteringServiceEvse;
};

}
}

#endif //MO_ENABLE_V201
#endif
