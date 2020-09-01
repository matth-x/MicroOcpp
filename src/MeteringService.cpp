// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#include "MeteringService.h"
#include "OcppOperation.h"
#include "MeterValues.h"
#include "OcppEngine.h"
#include "SimpleOcppOperationFactory.h"

#include <EEPROM.h>


MeteringService::MeteringService(WebSocketsClient *webSocket)
      : webSocket(webSocket) {
  powerMeasurementTime = LinkedList<time_t>();
  power = LinkedList<float>();

  energyMeasurementTime = LinkedList<time_t>();
  energy = LinkedList<float>();

  setMeteringSerivce(this); //make MeteringService available through Ocpp Engine

  /*
   * initialize EEPROM
   */
  EEPROM_Data data = {0};
  EEPROM.begin(sizeof(EEPROM_Data));
  EEPROM.get(0,data); //read all the data being saved to EEPROM to just use one value
  //data.energyActiveImportRegister = 0.0f; //TODO This line was added for consistency in the tests. Remove it afterwards.
  //EEPROM.put(0,data);
  EEPROM.end();
}

void MeteringService::addCurrentPowerDataPoint(float currentPower){
  //TODO If using this function, please revise it. Check new field mem_init_indicator in EEPROMLayout.h
  time_t currentTime = now();

  //Update Energy Register
  float energyActiveImportRegisterIncrement = 0.0f;
  if (lastSampleTime > 0                 //power already has been measured
      && lastSampleTime < currentTime) { //... and is not set to INFINITY
    time_t deltaTime = currentTime - lastSampleTime;
    float deltaTimeInSecs = ((float) deltaTime) / 1000.0f;
    energyActiveImportRegisterIncrement = deltaTimeInSecs * lastPower;
  }

  float energyActiveImportRegister = incrementEnergyActiveImportRegister(
                                              energyActiveImportRegisterIncrement);
  
  power.add(energyActiveImportRegister);
  powerMeasurementTime.add(currentTime);

  lastSampleTime = currentTime;
  lastPower = currentPower;

  /*
   * Check if to send all the meter values to the server
   */
  if (power.size() >= METER_VALUES_SAMPLED_DATA_MAX_LENGTH) {
    flushPowerValues();
  }
}

//TODO Remove logical separation of energy and power
void MeteringService::addEnergyDataPoint(float energyRegister){
  time_t currentTime = now();
  lastSampleTime = currentTime;

  //Is there even a transaction? If not, don't add a data point!
  if (getChargePointStatusService() != NULL) {
    if (getChargePointStatusService()->getTransactionId() < 0) {      
      flushEnergyValues();
      return;
    }
  }

  energy.add(energyRegister);
  energyMeasurementTime.add(currentTime);

  /*
   * Check if to send all the meter values to the server
   */
  if (energy.size() >= METER_VALUES_SAMPLED_DATA_MAX_LENGTH) {
    flushEnergyValues();
  }
}

void MeteringService::addEnergyAndPowerDataPoint(float energyRegister, float pwr) {
  time_t currentTime = now();
  lastSampleTime = currentTime;

  //Is there even a transaction? If not, don't add a data point!
  if (getChargePointStatusService() != NULL) {
    if (getChargePointStatusService()->getTransactionId() < 0) {      
      flushEnergyAndPowerValues();
      return;
    }
  }

  energy.add(energyRegister);
  power.add(pwr);
  powerMeasurementTime.add(currentTime);
  energyMeasurementTime.add(currentTime);

  /*
   * Check if to send all the meter values to the server
   */
  if (energyMeasurementTime.size() >= METER_VALUES_SAMPLED_DATA_MAX_LENGTH
      || powerMeasurementTime.size() >= METER_VALUES_SAMPLED_DATA_MAX_LENGTH) {
    flushEnergyAndPowerValues();
  }
}

void MeteringService::flushPowerValues() {
  if (power.size() == 0) return; //Nothing to report
  OcppOperation *meterValues = makeOcppOperation(webSocket,
    new MeterValues(&powerMeasurementTime, &power));
  initiateOcppOperation(meterValues);
  powerMeasurementTime.clear();
  power.clear();
}

void MeteringService::flushEnergyValues() {
  if (energy.size() == 0) return; //Nothing to report
  if (getChargePointStatusService() != NULL) {
    if (getChargePointStatusService()->getTransactionId() < 0) {      
      energyMeasurementTime.clear();
      energy.clear();
      return;
    }
  }
  OcppOperation *meterValues = makeOcppOperation(webSocket,
    new MeterValues(&energyMeasurementTime, &energy));
  initiateOcppOperation(meterValues);
  energyMeasurementTime.clear();
  energy.clear();
}

void MeteringService::flushEnergyAndPowerValues() {
  if (energy.size() == 0 && power.size() == 0) return; //Nothing to report
  if (getChargePointStatusService() != NULL) {
    if (getChargePointStatusService()->getTransactionId() < 0) {      
      energyMeasurementTime.clear();
      powerMeasurementTime.clear();
      energy.clear();
      power.clear();
      return;
    }
  }
  OcppOperation *meterValues = makeOcppOperation(webSocket,
    new MeterValues(&energyMeasurementTime, &energy, &power));
  initiateOcppOperation(meterValues);
  energyMeasurementTime.clear();
  powerMeasurementTime.clear();
  energy.clear();
  power.clear();
}

void MeteringService::loop(){

  /*
   * Calculate energy consumption which finally should be reportet to the Central Station in a MeterValues.req.
   * This code uses the EVSE's own energy register, if available (i.e. if energySampler is set). Otherwise it
   * uses the power sampler.
   * If no powerSampler is available, estimate the energy consumption taking the Charging Schedule and CP Status
   * into account.
   */
  if (now() >= (time_t) METER_VALUE_SAMPLE_INTERVAL + lastSampleTime) {
    if (energySampler != NULL) {
      float sampledEnergy = energySampler();
      //addEnergyDataPoint(sampledEnergy);
      if (powerSampler != NULL){
        float sampledPower = powerSampler();
        addEnergyAndPowerDataPoint(sampledEnergy, sampledPower);
      } else {
        addEnergyDataPoint(sampledEnergy);
      }
    } else {
      float sampledPower;
      if (powerSampler != NULL) {
        sampledPower = powerSampler();
      } else {
        //simulate a power value
        SmartChargingService *scService = getSmartChargingService();
        if (scService != NULL) {
            sampledPower = scService->inferenceLimitNow();
        } else {
          //just stick with simulated power data for the moment
          sampledPower = ((float)(now() % 320000)) / 10000.0f; //resulting value if not overriden by Smart Charging Service
                                                              //If Smart Charging Service is present, use max value instead 
        }
        ChargePointStatusService *cpStatusService = getChargePointStatusService();
        if (cpStatusService != NULL) {
          if (cpStatusService->inferenceStatus() != ChargePointStatus::Charging) {
            sampledPower = 0.0f; //Could cause trouble because this is a not so obvious side effect of a wrong state in cpStatusService
          }
        }
      }
      addCurrentPowerDataPoint(sampledPower);
    }
  }
}

//TODO This function is problematic on the ESP. Typical flash memories allow 1,000 to 10,000 write cycles. Find a better way
//TODO Updated EEPROMLayout. The first access in the lifetime of the ESP must initialize the memory and set mem_init_indicator
float MeteringService::incrementEnergyActiveImportRegister(float energy) {
  EEPROM_Data data = {0};
  EEPROM.begin(sizeof(EEPROM_Data));
  EEPROM.get(0,data); //read all the data being saved to EEPROM to just use one value
  data.energyActiveImportRegister += energy;
  float result = data.energyActiveImportRegister;
  EEPROM.put(0,data);
  //EEPROM.commit(); already called in EEPROM.end()
  EEPROM.end();
  return result;
}

//TODO Updated EEPROMLayout. The first access in the lifetime of the ESP must initialize the memory and set mem_init_indicator
float MeteringService::readEnergyActiveImportRegister() {
  if (energySampler != NULL) {
    return (float) energySampler();
  }
  EEPROM_Data data = {0};
  EEPROM.begin(sizeof(EEPROM_Data));
  EEPROM.get(0,data);
  float result = data.energyActiveImportRegister;
  EEPROM.end();
  return result;
}

void MeteringService::setPowerSampler(float (*ps)()){
  this->powerSampler = ps;
}

void MeteringService::setEnergySampler(float (*es)()){
  this->energySampler = es;
}
