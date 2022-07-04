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

    auto MeterValuesSampledData = declareConfiguration<const char*>(
        "MeterValuesSampledData",
        "Energy.Active.Import.Register,Power.Active.Import",
        CONFIGURATION_FN,
        true,true,true,false
    );
    MeterValuesSampledDataMaxLength = declareConfiguration("MeterValuesSampledDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    MeterValueSampleInterval = declareConfiguration("MeterValueSampleInterval", 60);
    
    auto StopTxnSampledData = declareConfiguration<const char*>(
        "StopTxnSampledData",
        "",
        CONFIGURATION_FN,
        true,true,true,false
    );
    StopTxnSampledDataMaxLength = declareConfiguration("StopTxnSampledDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    
    auto MeterValuesAlignedData = declareConfiguration<const char*>(
        "MeterValuesAlignedData",
        "Energy.Active.Import.Register,Power.Active.Import",
        CONFIGURATION_FN,
        true,true,true,false
    );
    MeterValuesAlignedDataMaxLength = declareConfiguration("MeterValuesAlignedDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    ClockAlignedDataInterval  = declareConfiguration("ClockAlignedDataInterval", 0);
    
    auto StopTxnAlignedData = declareConfiguration<const char*>(
        "StopTxnAlignedData",
        "",
        CONFIGURATION_FN,
        true,true,true,false
    );
    StopTxnAlignedDataMaxLength = declareConfiguration("StopTxnAlignedDataMaxLength", 4, CONFIGURATION_VOLATILE, false, true, false, false);
    
    sampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, MeterValuesSampledData));
    alignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, MeterValuesAlignedData));
    stopTxnSampledDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, StopTxnSampledData));
    stopTxnAlignedDataBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, StopTxnAlignedData));
}

OcppMessage *ConnectorMeterValuesRecorder::loop() {

    if (*ClockAlignedDataInterval >= 1) {

         if (alignedData.size() >= *MeterValuesAlignedDataMaxLength) {
            auto meterValues = new MeterValues(std::move(alignedData), connectorId, -1);
            alignedData.clear();
            return meterValues;
        }

        auto& timestampNow = context.getOcppTime().getOcppTimestampNow();
        auto dt = nextAlignedTime - timestampNow;
        if (dt <= 0 ||                              //normal case: interval elapsed
                dt > *ClockAlignedDataInterval) {   //special case: clock has been adjusted or first run

            AO_DBG_DEBUG("Clock aligned measurement %ds: %s", dt,
                abs(dt) <= 60 ?
                "in time (tolerance <= 60s)" : "off, e.g. because of first run. Ignore");
            if (abs(dt) <= 60) { //is measurement still "clock-aligned"?
                auto alignedMeterValues = alignedDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SampleClock);
                if (alignedMeterValues) {
                    alignedData.push_back(std::move(alignedMeterValues));
                }

                if (stopTxnAlignedData.size() + 1 < (size_t) (*StopTxnAlignedDataMaxLength)) {
                    //ensure that collection keeps one free data slot for final value at StopTransaction
                    auto alignedStopTx = stopTxnAlignedDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SampleClock);
                    if (alignedStopTx) {
                        stopTxnAlignedData.push_back(std::move(alignedStopTx));
                    }
                }
            }
            
            OcppTimestamp midnightBase = OcppTimestamp(2010,0,0,0,0,0);
            auto intervall = timestampNow - midnightBase;
            intervall %= 3600 * 24;
            OcppTimestamp midnight = timestampNow - intervall;
            intervall += *ClockAlignedDataInterval;
            if (intervall >= 3600 * 24) {
                //next measurement is tomorrow; set to precisely 00:00 
                nextAlignedTime = midnight;
                nextAlignedTime += 3600 * 24;
            } else {
                intervall /= *ClockAlignedDataInterval;
                nextAlignedTime = midnight + (intervall * *ClockAlignedDataInterval);
            }
        }
    } else {
        alignedData.clear();
        stopTxnAlignedData.clear();
    }

    if (*MeterValueSampleInterval >= 1) {
        //record periodic tx data

        if (sampledData.size() >= *MeterValuesSampledDataMaxLength) {
            auto meterValues = new MeterValues(std::move(sampledData), connectorId, lastTransactionId);
            sampledData.clear();
            return meterValues;
        }

        auto connector = context.getConnectorStatus(connectorId);
        if (connector && connector->getTransactionId() != lastTransactionId) {
            //transaction break

    	    //take a transaction-related sample which is aligned to the transaction break
            if (connector->getTransactionId() >= 0 && lastTransactionId < 0) {
                auto sampleStartTx = stopTxnSampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::TransactionBegin);
                if (sampleStartTx) {
                    stopTxnSampledData.push_back(std::move(sampleStartTx));
                }
            } else if (connector->getTransactionId() >= 0 && lastTransactionId > 0) {
                AO_DBG_ERR("Cannot switch txId");
            }

            MeterValues *meterValues = nullptr;
            if (!sampledData.empty()) {
                meterValues = new MeterValues(std::move(sampledData), connectorId, lastTransactionId);
                sampledData.clear();
            }
            lastTransactionId = connector->getTransactionId();
            lastSampleTime = ao_tick_ms();
            return meterValues;
        }

        if (ao_tick_ms() - lastSampleTime >= (ulong) (*MeterValueSampleInterval * 1000)) {
            auto sampleMeterValues = sampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SamplePeriodic);
            if (sampleMeterValues) {
                sampledData.push_back(std::move(sampleMeterValues));
            }

            if (stopTxnSampledData.size() + 1 < (size_t) (*StopTxnSampledDataMaxLength)) {
                //ensure that collection keeps one free data slot for final value at StopTransaction
                auto sampleStopTx = stopTxnSampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::SamplePeriodic);
                if (sampleStopTx) {
                    stopTxnSampledData.push_back(std::move(sampleStopTx));
                }
            }
            lastSampleTime = ao_tick_ms();
        }   

    } else {
        sampledData.clear();
        stopTxnSampledData.clear();
    }

    return nullptr; //successful method completition. Currently there is no reason to send a MeterValues Msg.
}

OcppMessage *ConnectorMeterValuesRecorder::takeTriggeredMeterValues() {

    auto sample = sampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::Trigger);

    if (!sample) {
        return nullptr;
    }

    int txId_now = -1;
    auto connector = context.getConnectorStatus(connectorId);
    if (connector) {
        txId_now = connector->getTransactionId();
    }

    decltype(sampledData) mv_now;
    mv_now.push_back(std::move(sample));

    return new MeterValues(std::move(mv_now), connectorId, txId_now);
}

void ConnectorMeterValuesRecorder::setPowerSampler(PowerSampler ps){
    this->powerSampler = ps;
}

void ConnectorMeterValuesRecorder::setEnergySampler(EnergySampler es){
    this->energySampler = es;
}

void ConnectorMeterValuesRecorder::addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    if (!meterValueSampler->getMeasurand().compare("Energy.Active.Import.Register")) {
        energySamplerIndex = samplers.size();
    }
    samplers.push_back(std::move(meterValueSampler));
}

std::unique_ptr<SampledValue> ConnectorMeterValuesRecorder::readTxEnergyMeter(ReadingContext reason) {
    if (energySamplerIndex >= 0 && energySamplerIndex < samplers.size()) {
        return samplers[energySamplerIndex]->takeValue(reason);
    } else {
        AO_DBG_DEBUG("Called readTxEnergyMeter(), but no energySampler or handling strategy set");
        return nullptr;
    }
}

std::vector<std::unique_ptr<MeterValue>> ConnectorMeterValuesRecorder::createStopTxMeterData() {
    
    //create final StopTxSample
    if (*MeterValueSampleInterval >= 1) { //... only if sampled Meter Values are activated
        auto sampleStopTx = stopTxnSampledDataBuilder->takeSample(context.getOcppTime().getOcppTimestampNow(), ReadingContext::TransactionEnd);
        if (sampleStopTx) {
            stopTxnSampledData.push_back(std::move(sampleStopTx));
        }
    }

    //concatenate sampled and aligned meter data; clear all StopTX data in this object
    auto res{std::move(stopTxnSampledData)};
    res.insert(res.end(), std::make_move_iterator(stopTxnAlignedData.begin()),
                          std::make_move_iterator(stopTxnAlignedData.end()));
    stopTxnSampledData.clear(); //make vectors defined after moving from them
    stopTxnAlignedData.clear();

    return std::move(res);
}
