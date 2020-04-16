#include "MeteringService.h"
#include "OcppOperation.h"
#include "MeterValues.h"
#include "OcppEngine.h"

#include <EEPROM.h>


MeteringService::MeteringService(WebSocketsClient *webSocket)
      : webSocket(webSocket) {
  powerMeasurementTime = LinkedList<time_t>();
  power = LinkedList<float>();

  setMeteringSerivce(this); //make MeteringService available through Ocpp Engine

  /*
   * initialize EEPROM
   */
  EEPROM_Data data = {0};
  EEPROM.begin(sizeof(EEPROM_Data));
  EEPROM.get(0,data); //read all the data being saved to EEPROM to just use one value
  data.energyActiveImportRegister = 0.0f; //TODO This line was added for consistency in the tests. Remove it afterwards.
  EEPROM.put(0,data);
  EEPROM.end();
}

void MeteringService::addCurrentPowerDataPoint(float currentPower){
  time_t currentTime = now();

  //Update Energy Register
  float energyActiveImportRegisterIncrement = 0.0f;
  if (lastPowerMeasurementTime > 0                 //power already has been measured
      && lastPowerMeasurementTime < currentTime) { //... and is not set to INFINITY
    time_t deltaTime = currentTime - lastPowerMeasurementTime;
    float deltaTimeInSecs = ((float) deltaTime) / 1000.0f;
    energyActiveImportRegisterIncrement = deltaTimeInSecs * lastPower;
  }

  float energyActiveImportRegister = incrementEnergyActiveImportRegister(
                                              energyActiveImportRegisterIncrement);
  
  power.add(energyActiveImportRegister);
  powerMeasurementTime.add(currentTime);

  lastPowerMeasurementTime = currentTime;
  lastPower = currentPower;

  /*
   * Check if to send all the meter values to the server
   */
  if (power.size() >= METER_VALUES_SAMPLED_DATA_MAX_LENGTH) {
    flushPowerValues();
  }
}

void MeteringService::flushPowerValues() {
  if (power.size() == 0) return; //Nothing to report
  OcppOperation *meterValues = new MeterValues(webSocket, &powerMeasurementTime, &power);
  initiateOcppOperation(meterValues);
  powerMeasurementTime.clear();
  power.clear();
}

void MeteringService::loop(){

  /*
   * Calculate energy consumption which finally should be reportet to the Central Station in a MeterValues.req.
   * If no powerSampler is available, estimate the energy consumption taking the Charging Schedule and CP Status
   * into account.
   */
  if (now() > (time_t) METER_VALUE_SAMPLE_INTERVAL + lastPowerMeasurementTime) {
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


float MeteringService::readEnergyActiveImportRegister() {
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
