// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

MeteringService::MeteringService(Context& context, int numConn, std::shared_ptr<FilesystemAdapter> filesystem)
      : MemoryManaged("v16.Metering.MeteringService"), context(context), meterStore(filesystem), connectors(makeVector<std::unique_ptr<MeteringConnector>>(getMemoryTag())) {

    //set factory defaults for Metering-related config keys
    declareConfiguration<const char*>("MeterValuesSampledData", "Energy.Active.Import.Register,Power.Active.Import");
    declareConfiguration<const char*>("StopTxnSampledData", "");
    declareConfiguration<const char*>("MeterValuesAlignedData", "Energy.Active.Import.Register,Power.Active.Import");
    declareConfiguration<const char*>("StopTxnAlignedData", "");
    
    connectors.reserve(numConn);
    for (int i = 0; i < numConn; i++) {
        connectors.emplace_back(new MeteringConnector(context, i, meterStore));
    }

    std::function<bool(const char*)> validateSelectString = [this] (const char *csl) {
        bool isValid = true;
        const char *l = csl; //the beginning of an entry of the comma-separated list
        const char *r = l; //one place after the last character of the entry beginning with l
        while (*l) {
            if (*l == ',') {
                l++;
                continue;
            }
            r = l + 1;
            while (*r != '\0' && *r != ',') {
                r++;
            }
            bool found = false;
            for (size_t cId = 0; cId < connectors.size(); cId++) {
                if (connectors[cId]->existsSampler(l, (size_t) (r - l))) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                isValid = false;
                MO_DBG_WARN("could not find metering device for %.*s", (int) (r - l), l);
                break;
            }
            l = r;
        }
        return isValid;
    };

    std::function<bool(const char*)> validateUnsignedIntString = [] (const char *value) {
        for(size_t i = 0; value[i] != '\0'; i++)
        {
            if (value[i] < '0' || value[i] > '9') {
                return false;
            }
        }
        return true;
    };

    registerConfigurationValidator("MeterValuesSampledData", validateSelectString);
    registerConfigurationValidator("StopTxnSampledData", validateSelectString);
    registerConfigurationValidator("MeterValuesAlignedData", validateSelectString);
    registerConfigurationValidator("StopTxnAlignedData", validateSelectString);
    registerConfigurationValidator("MeterValueSampleInterval", validateUnsignedIntString);
    registerConfigurationValidator("ClockAlignedDataInterval", validateUnsignedIntString);

    /*
     * Register further message handlers to support echo mode: when this library
     * is connected with a WebSocket echo server, let it reply to its own requests.
     * Mocking an OCPP Server on the same device makes running (unit) tests easier.
     */
    context.getOperationRegistry().registerOperation("MeterValues", [this] () {
        return new Ocpp16::MeterValues(this->context.getModel());});
}

void MeteringService::loop(){
    for (unsigned int i = 0; i < connectors.size(); i++){
        connectors[i]->loop();
    }
}

void MeteringService::addMeterValueSampler(int connectorId, std::unique_ptr<SampledValueSampler> meterValueSampler) {
    if (connectorId < 0 || connectorId >= (int) connectors.size()) {
        MO_DBG_ERR("connectorId is out of bounds");
        return;
    }
    connectors[connectorId]->addMeterValueSampler(std::move(meterValueSampler));
}

std::unique_ptr<SampledValue> MeteringService::readTxEnergyMeter(int connectorId, ReadingContext context) {
    if (connectorId < 0 || (size_t) connectorId >= connectors.size()) {
        MO_DBG_ERR("connectorId is out of bounds");
        return nullptr;
    }
    return connectors[connectorId]->readTxEnergyMeter(context);
}

std::unique_ptr<Request> MeteringService::takeTriggeredMeterValues(int connectorId) {
    if (connectorId < 0 || connectorId >= (int) connectors.size()) {
        MO_DBG_ERR("connectorId out of bounds. Ignore");
        return nullptr;
    }

    auto msg = connectors[connectorId]->takeTriggeredMeterValues();
    if (msg) {
        auto meterValues = makeRequest(std::move(msg));
        meterValues->setTimeout(120000);
        return meterValues;
    }
    MO_DBG_DEBUG("Did not take any samples for connectorId %d", connectorId);
    return nullptr;
}

void MeteringService::beginTxMeterData(Transaction *transaction) {
    if (!transaction) {
        MO_DBG_ERR("invalid argument");
        return;
    }
    auto connectorId = transaction->getConnectorId();
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("connectorId is out of bounds");
        return;
    }
    connectors[connectorId]->beginTxMeterData(transaction);
}

std::shared_ptr<TransactionMeterData> MeteringService::endTxMeterData(Transaction *transaction) {
    if (!transaction) {
        MO_DBG_ERR("invalid argument");
        return nullptr;
    }
    auto connectorId = transaction->getConnectorId();
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("connectorId is out of bounds");
        return nullptr;
    }
    return connectors[connectorId]->endTxMeterData(transaction);
}

void MeteringService::abortTxMeterData(unsigned int connectorId) {
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("connectorId is out of bounds");
        return;
    }
    connectors[connectorId]->abortTxMeterData();
}

std::shared_ptr<TransactionMeterData> MeteringService::getStopTxMeterData(Transaction *transaction) {
    if (!transaction) {
        MO_DBG_ERR("invalid argument");
        return nullptr;
    }
    auto connectorId = transaction->getConnectorId();
    if (connectorId >= connectors.size()) {
        MO_DBG_ERR("connectorId is out of bounds");
        return nullptr;
    }
    return connectors[connectorId]->getStopTxMeterData(transaction);
}

bool MeteringService::removeTxMeterData(unsigned int connectorId, unsigned int txNr) {
    return meterStore.remove(connectorId, txNr);
}

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Variables/VariableService.h>

namespace MicroOcpp {
namespace Ocpp201 {

MeteringServiceEvse::MeteringServiceEvse(Model& model, MeterStore& meterStore, unsigned int evseId) : MemoryManaged("v201.Metering.MeteringServiceEvse"), model(model), meterStore(meterStore), evseId(evseId), samplers(makeVector<std::unique_ptr<SampledValueSampler>>(getMemoryTag())) {

    auto varService = model.getVariableService();

    auto sampledDataTxStartedMeasurandsString = varService->declareVariable<const char*>("SampledDataCtrlr", "TxStartedMeasurands", "");
    auto sampledDataTxUpdatedMeasurandsString = varService->declareVariable<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", "");
    auto sampledDataTxEndedMeasurandsString = varService->declareVariable<const char*>("SampledDataCtrlr", "TxEndedMeasurands", "");

    sampledDataTxStartedBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, sampledDataTxStartedMeasurandsString));
    sampledDataTxUpdatedBuilder = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, sampledDataTxUpdatedMeasurandsString));
    sampledDataTxEndedBuilder   = std::unique_ptr<MeterValueBuilder>(new MeterValueBuilder(samplers, sampledDataTxEndedMeasurandsString));
}

void MeteringServiceEvse::addMeterValueSampler(std::unique_ptr<SampledValueSampler> meterValueSampler) {
    samplers.push_back(std::move(meterValueSampler));
}

std::unique_ptr<MeterValue> MeteringServiceEvse::takeTxStartedMeterValue(ReadingContext context) {
    return sampledDataTxStartedBuilder->takeSample(model.getClock().now(), context);
}

std::unique_ptr<MeterValue> MeteringServiceEvse::takeTxUpdatedMeterValue(ReadingContext context) {
    return sampledDataTxUpdatedBuilder->takeSample(model.getClock().now(), context);
}

std::unique_ptr<MeterValue> MeteringServiceEvse::takeTxEndedMeterValue(ReadingContext context) {
    return sampledDataTxEndedBuilder->takeSample(model.getClock().now(), context);
}

std::unique_ptr<MeterValue> MeteringServiceEvse::takeTriggeredMeterValues() {
    return sampledDataTxUpdatedBuilder->takeSample(model.getClock().now(), ReadingContext::Trigger);
}

std::shared_ptr<TransactionMeterData> MeteringServiceEvse::getTxMeterData(unsigned int txNr) {
    return meterStore.getTxMeterData(samplers, evseId, txNr);
}

bool MeteringServiceEvse::existsSampler(const char *measurand, size_t len) {
    for (size_t i = 0; i < samplers.size(); i++) {
        if (strlen(samplers[i]->getProperties().getMeasurand()) == len &&
                !strncmp(measurand, samplers[i]->getProperties().getMeasurand(), len)) {
            return true;
        }
    }

    return false;
}

MeteringService::MeteringService(Model& model, size_t numEvses, std::shared_ptr<FilesystemAdapter> filesystem) : MemoryManaged("v201.Metering.MeteringService"), model(model), meterStore(filesystem) {

    auto varService = model.getVariableService();

    //define factory defaults
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxStartedMeasurands", "");
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", "");
    varService->declareVariable<const char*>("SampledDataCtrlr", "TxEndedMeasurands", "");

    std::function<bool(const char*)> validateSelectString = [this] (const char *csl) {
        bool isValid = true;
        const char *l = csl; //the beginning of an entry of the comma-separated list
        const char *r = l; //one place after the last character of the entry beginning with l
        while (*l) {
            if (*l == ',') {
                l++;
                continue;
            }
            r = l + 1;
            while (*r != '\0' && *r != ',') {
                r++;
            }
            bool found = false;
            for (size_t evseId = 0; evses[evseId] && evseId < MO_NUM_EVSE; evseId++) {
                if (evses[evseId]->existsSampler(l, (size_t) (r - l))) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                isValid = false;
                MO_DBG_WARN("could not find metering device for %.*s", (int) (r - l), l);
                break;
            }
            l = r;
        }
        return isValid;
    };

    varService->registerValidator<const char*>("SampledDataCtrlr", "TxStartedMeasurands", validateSelectString);
    varService->registerValidator<const char*>("SampledDataCtrlr", "TxUpdatedMeasurands", validateSelectString);
    varService->registerValidator<const char*>("SampledDataCtrlr", "TxEndedMeasurands", validateSelectString);

    for (size_t i = 0; i < numEvses && i < MO_NUM_EVSE; i++) {
        evses[i] = new MeteringServiceEvse(model, meterStore, (unsigned int)i);
    }
}
MeteringService::~MeteringService() {
    for (size_t evseId = 0; evses[evseId] && evseId < MO_NUM_EVSE; evseId++) {
        delete evses[evseId];
    }
}

MeteringServiceEvse *MeteringService::getEvse(unsigned int evseId) {
    if (evseId >= MO_NUM_EVSE) {
        return nullptr;
    }
    return evses[evseId];
}

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
