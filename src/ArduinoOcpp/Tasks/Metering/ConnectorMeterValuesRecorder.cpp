// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/Metering/ConnectorMeterValuesRecorder.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

ConnectorMeterValuesRecorder::ConnectorMeterValuesRecorder(int connectorId, OcppTime *ocppTime)
        : connectorId(connectorId), ocppTime(ocppTime) {
    sampleTimestamp = std::vector<OcppTimestamp>();
    energy = std::vector<float>();
    power = std::vector<float>();

    MeterValueSampleInterval = declareConfiguration("MeterValueSampleInterval", 60);
    MeterValuesSampledDataMaxLength = declareConfiguration("MeterValuesSampledDataMaxLength", 4);
}

void ConnectorMeterValuesRecorder::takeSample() {
    if (energySampler != NULL || powerSampler != NULL) {
        if (!ocppTime->isValid()) return;
        sampleTimestamp.push_back(ocppTime->getOcppTimestampNow());
    }

    if (energySampler != NULL) {
        energy.push_back(energySampler());
    }

    if (powerSampler != NULL) {
        power.push_back(powerSampler());
    }
}

MeterValues *ConnectorMeterValuesRecorder::loop() {

    /*
     * First: check if there was a transaction break (i.e. transaction either started or stopped; transactionId changed)
     */ 
    ConnectorStatus *connector = getConnectorStatus(connectorId);
    if (connector->getTransactionId() != lastTransactionId) {
        //transaction break occured!
        MeterValues *result = toMeterValues();
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
    if (millis() - lastSampleTime >= (ulong) (*MeterValueSampleInterval * 1000)) {
        takeSample();
        lastSampleTime = millis();
    }


    /*
    * Is the value buffer already full? If yes, return MeterValues message
    */
    if (((int) sampleTimestamp.size()) >= (int) *MeterValuesSampledDataMaxLength) {
        MeterValues *result = toMeterValues();
        return result;
    }

    return NULL; //successful method completition. Currently there is no reason to send a MeterValues Msg.
}

MeterValues *ConnectorMeterValuesRecorder::toMeterValues() {
    if (sampleTimestamp.size() == 0) {
        //Switching from Non-Transaction to Transaction (or vice versa) without sample. Or anything wrong here. Discard
        if (DEBUG_OUT) Serial.print(F("[ConnectorMeterValuesRecorder] Try to send MeterValues without any data point. Ignore\n"));
        clear();
        return NULL;
    }

    //decide which measurands to send. If a measurand is missing at at least one point in time, omit that measurand completely

    if (energy.size() == sampleTimestamp.size() && power.size() == sampleTimestamp.size()) {
        MeterValues *result = new MeterValues(&sampleTimestamp, &energy, &power, connectorId, lastTransactionId);
        clear();
        return result;
    }

    if (energy.size() == sampleTimestamp.size() && power.size() != sampleTimestamp.size()) {
        MeterValues *result = new MeterValues(&sampleTimestamp, &energy, NULL, connectorId, lastTransactionId);
        clear();
        return result;
    }

    if (energy.size() != sampleTimestamp.size() && power.size() == sampleTimestamp.size()) {
        MeterValues *result = new MeterValues(&sampleTimestamp, NULL, &power, connectorId, lastTransactionId);
        clear();
        return result;
    }

    //Maybe the energy sampler or power sampler was set during recording. Discard recorded data.
    Serial.print(F("[ConnectorMeterValuesRecorder] Invalid data set. Discard data set and restard recording.\n"));
    clear();

    return NULL;
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
    if (energySampler != NULL) {
        return energySampler();
    } else {
        Serial.print(F("[ConnectorMeterValuesRecorder] Called readEnergyActiveImportRegister(), but no energySampler or handling strategy set!\n"));
        return 0.f;
    }
}
