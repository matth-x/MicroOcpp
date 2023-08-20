// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHARGECONTROLCOMMON_H
#define CHARGECONTROLCOMMON_H

#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {

class Context;

class ConnectorsCommon {
private:
    Context& context;
public:
    ConnectorsCommon(Context& context, unsigned int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();
};

} //end namespace MicroOcpp

#endif
