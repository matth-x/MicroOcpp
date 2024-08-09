// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERSTORE_H
#define MO_METERSTORE_H

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class TransactionMeterData : public AllocOverrider {
private:
    const unsigned int connectorId; //assignment to Transaction object
    const unsigned int txNr; //assignment to Transaction object

    unsigned int mvCount = 0; //nr of saved meter values, including gaps
    bool finalized = false; //if true, this is read-only

    std::shared_ptr<FilesystemAdapter> filesystem;

    MemVector<std::unique_ptr<MeterValue>> txData;

public:
    TransactionMeterData(unsigned int connectorId, unsigned int txNr, std::shared_ptr<FilesystemAdapter> filesystem);

    bool addTxData(std::unique_ptr<MeterValue> mv);

    MemVector<std::unique_ptr<MeterValue>> retrieveStopTxData(); //will invalidate internal cache

    bool restore(MeterValueBuilder& mvBuilder); //load record from memory; true if record found, false if nothing loaded

    unsigned int getConnectorId() {return connectorId;}
    unsigned int getTxNr() {return txNr;}
    unsigned int getPathsCount() {return mvCount;} //size of spanned path indexes
    void finalize() {finalized = true;}
    bool isFinalized() {return finalized;}
};

class MeterStore : public AllocOverrider {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;
    
    MemVector<std::weak_ptr<TransactionMeterData>> txMeterData;

public:
    MeterStore() = delete;
    MeterStore(MeterStore&) = delete;
    MeterStore(std::shared_ptr<FilesystemAdapter> filesystem);

    std::shared_ptr<TransactionMeterData> getTxMeterData(MeterValueBuilder& mvBuilder, Transaction *transaction);

    bool remove(unsigned int connectorId, unsigned int txNr);
};

}

#endif
