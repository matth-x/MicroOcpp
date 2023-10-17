// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Operations/MeterValues.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

MeteringService::MeteringService(Context& context, int numConn, std::shared_ptr<FilesystemAdapter> filesystem)
      : context(context), meterStore(filesystem) {

    //set factory defaults for Metering-related config keys
    declareConfiguration<const char*>("MeterValuesSampledData", "Energy.Active.Import.Register,Power.Active.Import");
    declareConfiguration<const char*>("StopTxnSampledData", "");
    declareConfiguration<const char*>("MeterValuesAlignedData", "Energy.Active.Import.Register,Power.Active.Import");
    declareConfiguration<const char*>("StopTxnAlignedData", "");
    
    for (int i = 0; i < numConn; i++) {
        connectors.emplace_back(new MeteringConnector(context.getModel(), i, meterStore));
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
    registerConfigurationValidator("MeterValuesSampledData", validateSelectString);
    registerConfigurationValidator("StopTxnSampledData", validateSelectString);
    registerConfigurationValidator("MeterValuesAlignedData", validateSelectString);
    registerConfigurationValidator("StopTxnAlignedData", validateSelectString);

    /*
     * Register further message handlers to support echo mode: when this library
     * is connected with a WebSocket echo server, let it reply to its own requests.
     * Mocking an OCPP Server on the same device makes running (unit) tests easier.
     */
    context.getOperationRegistry().registerOperation("MeterValues", [] () {
        return new Ocpp16::MeterValues();});
}

void MeteringService::loop(){

    for (unsigned int i = 0; i < connectors.size(); i++){
        auto meterValuesMsg = connectors[i]->loop();
        if (meterValuesMsg != nullptr) {
            auto meterValues = makeRequest(std::move(meterValuesMsg));
            meterValues->setTimeout(120000);
            context.initiateRequest(std::move(meterValues));
        }
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
    auto& connector = connectors.at(connectorId);
    if (connector.get()) {
        auto msg = connector->takeTriggeredMeterValues();
        if (msg) {
            auto meterValues = makeRequest(std::move(msg));
            meterValues->setTimeout(120000);
            return meterValues;
        }
        MO_DBG_DEBUG("Did not take any samples for connectorId %d", connectorId);
        return nullptr;
    }
    MO_DBG_ERR("Could not find connector");
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
    auto& connector = connectors[connectorId];

    connector->beginTxMeterData(transaction);
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
    auto& connector = connectors[connectorId];

    return connector->endTxMeterData(transaction);
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
    auto& connector = connectors[connectorId];

    return connector->getStopTxMeterData(transaction);
}

bool MeteringService::removeTxMeterData(unsigned int connectorId, unsigned int txNr) {
    return meterStore.remove(connectorId, txNr);
}
