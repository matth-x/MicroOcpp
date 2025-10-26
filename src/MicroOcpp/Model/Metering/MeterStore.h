// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERSTORE_H
#define MO_METERSTORE_H

#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

#ifndef MO_STOPTXDATA_MAX_SIZE
#define MO_STOPTXDATA_MAX_SIZE 4
#endif

#ifndef MO_STOPTXDATA_DIGITS
#define MO_STOPTXDATA_DIGITS 1 //digits needed to print MO_STOPTXDATA_MAX_SIZE-1 (="3", i.e. 1 digit)
#endif

namespace MicroOcpp {

class Context;

namespace v16 {
namespace MeterStore {

bool printTxMeterValueFName(char *fname, size_t size, unsigned int evseId, unsigned int txNr, unsigned int mvIndex);

FilesystemUtils::LoadStatus load(MO_FilesystemAdapter *filesystem, Context& context, Vector<MO_MeterInput>& meterInputs, unsigned int evseId, unsigned int txNr, Vector<MeterValue*>& txMeterValue);
FilesystemUtils::StoreStatus store(MO_FilesystemAdapter *filesystem, Context& context, unsigned int evseId, unsigned int txNr, unsigned int mvIndex, MeterValue& mv);
bool remove(MO_FilesystemAdapter *filesystem, unsigned int evseId, unsigned int txNr); //removes tx meter values only

} //namespace MeterStore
} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
