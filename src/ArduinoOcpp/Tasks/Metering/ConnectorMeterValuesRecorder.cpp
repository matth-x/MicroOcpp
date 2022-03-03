// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/MessagesV16/MeterValues.h>
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorMeterValuesRecorder::ConnectorMeterValuesRecorder(OcppModel& context, int connectorId)
        : context(context), connectorId{connectorId} {
    sampleTimestamp = std::vector<OcppTimestamp>();
    energy = std::vector<float>();
    power = std::vector<float>();

    MeterValueSampleInterval = declareConfiguration("MeterValueSampleInterval", 60);
    MeterValuesSampledDataMaxLength = declareConfiguration("MeterValuesSampledDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
}

void ConnectorMeterValuesRecorder::takeSample() {
    if (energySampler != nullptr || powerSampler != nullptr) {
        if (!context.getOcppTime().isValid()) return;
        sampleTimestamp.push_back(context.getOcppTime().getOcppTimestampNow());
    }

    if (energySampler != nullptr) {
        energy.push_back(energySampler());
    }

    if (powerSampler != nullptr) {
        power.push_back(powerSampler());
    }
}

OcppMessage *ConnectorMeterValuesRecorder::loop() {

    if (*MeterValueSampleInterval < 1) {
        //Metering off by definition
        clear();
        return nullptr;
    }

    /*
     * First: check if there was a transaction break (i.e. transaction either started or stopped; transactionId changed)
     */ 
    auto connector = context.getConnectorStatus(connectorId);
    if (connector && connector->getTransactionId() != lastTransactionId) {
        //transaction break occured!
        auto result = toMeterValues();
        lastTransactionId = connector->getTransactionId();
        return result;
    }

    /*
    * Calculate energy consumption which finally should be reportet to the Central Station in a MeterValues.req.
    * This code uses the EVSE's own energy register, if available (i.e. if energySampler is set). Otherwise it
    * uses the power sampler.
    * If no powerSampler is available, estimate the energy consumption taking the Charging Schedule and CP Status
    * into account.
    */
    if (ao_tick_ms() - lastSampleTime >= (ulong) (*MeterValueSampleInterval * 1000)) {
        takeSample();
        lastSampleTime = ao_tick_ms();
    }


    /*
    * Is the value buffer already full? If yes, return MeterValues message
    */
    if (((int) sampleTimestamp.size()) >= (int) *MeterValuesSampledDataMaxLength) {
        auto result = toMeterValues();
        return result;
    }

    return nullptr; //successful method completition. Currently there is no reason to send a MeterValues Msg.
}

OcppMessage *ConnectorMeterValuesRecorder::toMeterValues() {
    if (sampleTimestamp.size() == 0) {
        AO_DBG_DEBUG("Checking if to send MeterValues ... No");
        clear();
        return nullptr;
    }

    //decide which measurands to send. If a measurand is missing at at least one point in time, omit that measurand completely

    if (energy.size() == sampleTimestamp.size() && power.size() == sampleTimestamp.size()) {
        auto result = new MeterValues(&sampleTimestamp, &energy, &power, connectorId, lastTransactionId);
        clear();
        return result;
    }

    if (energy.size() == sampleTimestamp.size() && power.size() != sampleTimestamp.size()) {
        auto result = new MeterValues(&sampleTimestamp, &energy, nullptr, connectorId, lastTransactionId);
        clear();
        return result;
    }

    if (energy.size() != sampleTimestamp.size() && power.size() == sampleTimestamp.size()) {
        auto result = new MeterValues(&sampleTimestamp, nullptr, &power, connectorId, lastTransactionId);
        clear();
        return result;
    }

    //Maybe the energy sampler or power sampler was set during recording. Discard recorded data.
    AO_DBG_WARN("Invalid data set. Discard data set and restart recording");
    clear();

    return nullptr;
}

OcppMessage *ConnectorMeterValuesRecorder::takeMeterValuesNow() {

    if (!energySampler && !powerSampler) {
        return nullptr;
    }

    decltype(sampleTimestamp) t_now;
    decltype(energy) e_now;
    decltype(power) p_now;

    if (context.getOcppTime().isValid()) {
        t_now.push_back(context.getOcppTime().getOcppTimestampNow());
    }

    if (energySampler) {
        e_now.push_back(energySampler());
    }

    if (powerSampler) {
        p_now.push_back(powerSampler());
    }

    int txId_now = -1;
    auto connector = context.getConnectorStatus(connectorId);
    if (connector) {
        txId_now = connector->getTransactionId();
    }

    return new MeterValues(&t_now, &e_now, &p_now, connectorId, txId_now);
}

void ConnectorMeterValuesRecorder::clear() {
    sampleTimestamp.clear();
    energy.clear();
    power.clear();
}

void ConnectorMeterValuesRecorder::setPowerSampler(PowerSampler ps){
  this->powerSampler = ps;
}

void ConnectorMeterValuesRecorder::setEnergySampler(EnergySampler es){
  this->energySampler = es;
}

float ConnectorMeterValuesRecorder::readEnergyActiveImportRegister() {
    if (energySampler != nullptr) {
        return energySampler();
    } else {
        AO_DBG_DEBUG("Called readEnergyActiveImportRegister(), but no energySampler or handling strategy set");
        return 0.f;
    }
}
