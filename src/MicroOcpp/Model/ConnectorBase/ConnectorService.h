// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CHARGECONTROLCOMMON_H
#define MO_CHARGECONTROLCOMMON_H

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;

class ConnectorService : public MemoryManaged {
private:
    Context& context;
public:
    ConnectorService(Context& context, unsigned int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();
};

} //end namespace MicroOcpp

#endif
