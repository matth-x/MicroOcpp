// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONNECTOR_METER_VALUES_RECORDER
#define CONNECTOR_METER_VALUES_RECORDER

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <LinkedList.h>
#include <TimeHelper.h>
#include <MeterValues.h>
#include "Configuration.h"

typedef float (*PowerSampler)();
typedef float (*EnergySampler)();

class ConnectorMeterValuesRecorder {
private:
    const int connectorId;

    LinkedList<time_t> sampleTimestamp;
    LinkedList<float> energy;
    LinkedList<float> power;
    time_t lastSampleTime = 0; //0 means not charging right now
    float lastPower;
    int lastTransactionId = -1;
 
    float (*powerSampler)() = NULL;
    float (*energySampler)() = NULL;

    //ulong MeterValueSampleInterval = 60; //will be overwritten (see constructor)
    //ulong MeterValuesSampledDataMaxLength = 4; //will be overwritten (see constructor)
    std::shared_ptr<Configuration<int>> MeterValueSampleInterval = NULL;
    std::shared_ptr<Configuration<int>> MeterValuesSampledDataMaxLength = NULL;

    void takeSample();
    MeterValues *toMeterValues();
    void clear();
public:
    ConnectorMeterValuesRecorder(int connectorId);

    MeterValues *loop();

    void setPowerSampler(PowerSampler powerSampler);

    void setEnergySampler(EnergySampler energySampler);

    float readEnergyActiveImportRegister();
};

#endif