// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef METERSTORE_H
#define METERSTORE_H

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

#include <vector>
#include <deque>

namespace MicroOcpp {

class TransactionMeterData {
private:
    const unsigned int connectorId; //assignment to Transaction object
    const unsigned int txNr; //assignment to Transaction object

    unsigned int mvCount = 0; //nr of saved meter values
    bool finalized = false; //if true, this is read-only

    std::shared_ptr<FilesystemAdapter> filesystem;

    std::vector<std::unique_ptr<MeterValue>> txData;

public:
    TransactionMeterData(unsigned int connectorId, unsigned int txNr, std::shared_ptr<FilesystemAdapter> filesystem);

    bool addTxData(std::unique_ptr<MeterValue> mv);

    std::vector<std::unique_ptr<MeterValue>> retrieveStopTxData(); //will invalidate internal cache

    bool restore(MeterValueBuilder& mvBuilder); //load record from memory; true if record found, false if nothing loaded

    unsigned int getConnectorId() {return connectorId;}
    unsigned int getTxNr() {return txNr;}
    unsigned int getPathsCount() {return mvCount;} //size of spanned path indexes
    void finalize() {finalized = true;}
    bool isFinalized() {return finalized;}
};

class MeterStore {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;
    
    std::vector<std::weak_ptr<TransactionMeterData>> txMeterData;

public:
    MeterStore() = delete;
    MeterStore(MeterStore&) = delete;
    MeterStore(std::shared_ptr<FilesystemAdapter> filesystem);

    std::shared_ptr<TransactionMeterData> getTxMeterData(MeterValueBuilder& mvBuilder, Transaction *transaction);

    bool remove(unsigned int connectorId, unsigned int txNr);
};

}

#endif
