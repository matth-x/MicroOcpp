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

    MeterValueSampleInterval = declareConfiguration("MeterValueSampleInterval", 60);
    MeterValuesSampledDataMaxLength = declareConfiguration("MeterValuesSampledDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
}

void ConnectorMeterValuesRecorder::takeSample() {
    if (meterValueSamplers.empty()) return;

    std::unique_ptr<MeterValue> sample;
    if (context.getOcppTime().isValid()) {
        sample.reset(new MeterValue(context.getOcppTime().getOcppTimestampNow()));
    }
    if (!sample) {
        return;
    }

    for (auto mvs = meterValueSamplers.begin(); mvs != meterValueSamplers.end(); mvs++) {
        sample->addSampledValue((*mvs)->takeValue());
    }

    meterValue.push_back(std::move(sample));
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
    if (((int) meterValue.size()) >= (int) *MeterValuesSampledDataMaxLength) {
        auto result = toMeterValues();
        return result;
    }

    return nullptr; //successful method completition. Currently there is no reason to send a MeterValues Msg.
}

OcppMessage *ConnectorMeterValuesRecorder::toMeterValues() {
    if (meterValue.empty()) {
        AO_DBG_DEBUG("Checking if to send MeterValues ... No");
        clear();
        return nullptr;
    } else {
        auto result = new MeterValues(meterValue, connectorId, lastTransactionId);
        clear();
        return result;
    }
}

OcppMessage *ConnectorMeterValuesRecorder::takeMeterValuesNow() {

    if (meterValueSamplers.empty()) {
        return nullptr;
    }

    std::unique_ptr<MeterValue> value;

    if (context.getOcppTime().isValid()) {
        value.reset(new MeterValue(context.getOcppTime().getOcppTimestampNow()));
    }

    if (!value) {
        return nullptr;
    }

    for (auto mvs = meterValueSamplers.begin(); mvs != meterValueSamplers.end(); mvs++) {
        value->addSampledValue((*mvs)->takeValue());
    }

    int txId_now = -1;
    auto connector = context.getConnectorStatus(connectorId);
    if (connector) {
        txId_now = connector->getTransactionId();
    }

    decltype(meterValue) mv_now;
    mv_now.push_back(std::move(value));

    return new MeterValues(mv_now, connectorId, txId_now);
}

void ConnectorMeterValuesRecorder::clear() {
    meterValue.clear();
}

void ConnectorMeterValuesRecorder::setPowerSampler(PowerSampler ps){
    this->powerSampler = ps;
}

void ConnectorMeterValuesRecorder::setEnergySampler(EnergySampler es){
    this->energySampler = es;
}

void ConnectorMeterValuesRecorder::addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    meterValueSamplers.push_back(std::move(meterValueSampler));
}

int32_t ConnectorMeterValuesRecorder::readEnergyActiveImportRegister() {
    if (energySampler != nullptr) {
        return energySampler();
    } else {
        AO_DBG_DEBUG("Called readEnergyActiveImportRegister(), but no energySampler or handling strategy set");
        return 0.f;
    }
}
