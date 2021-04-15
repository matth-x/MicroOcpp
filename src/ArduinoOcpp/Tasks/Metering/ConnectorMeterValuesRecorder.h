// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONNECTOR_METER_VALUES_RECORDER
#define CONNECTOR_METER_VALUES_RECORDER

//#define METER_VALUE_SAMPLE_INTERVAL 60 //in seconds

//#define METER_VALUES_SAMPLED_DATA_MAX_LENGTH 4 //after 4 measurements, send the values to the CS

#include <LinkedList.h>
#include <ArduinoOcpp/TimeHelper.h>
#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Core/Configuration.h>

namespace ArduinoOcpp {

//typedef float (*PowerSampler)();
//typedef float (*EnergySampler)();
typedef std::function<float()> PowerSampler;
typedef std::function<float()> EnergySampler;

class ConnectorMeterValuesRecorder {
private:
    const int connectorId;

    LinkedList<time_t> sampleTimestamp;
    LinkedList<float> energy;
    LinkedList<float> power;
    time_t lastSampleTime = 0; //0 means not charging right now
    float lastPower;
    int lastTransactionId = -1;
 
    PowerSampler powerSampler = NULL;
    EnergySampler energySampler = NULL;

    //ulong MeterValueSampleInterval = 60; //will be overwritten (see constructor)
    //ulong MeterValuesSampledDataMaxLength = 4; //will be overwritten (see constructor)
    std::shared_ptr<Configuration<int>> MeterValueSampleInterval = NULL;
    std::shared_ptr<Configuration<int>> MeterValuesSampledDataMaxLength = NULL;

    void takeSample();
    Ocpp16::MeterValues *toMeterValues();
    void clear();
public:
    ConnectorMeterValuesRecorder(int connectorId);

    Ocpp16::MeterValues *loop();

    void setPowerSampler(PowerSampler powerSampler);

    void setEnergySampler(EnergySampler energySampler);

    float readEnergyActiveImportRegister();
};

} //end namespace ArduinoOcpp
#endif