// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#ifndef MO_VARIABLECONTAINER_H
#define MO_VARIABLECONTAINER_H

#include <memory>

#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

class VariableContainer {
public:
    ~VariableContainer();
    virtual size_t size() = 0;
    virtual Variable *getVariable(size_t i) = 0;
    virtual Variable *getVariable(const ComponentId& component, const char *variableName) = 0;

    virtual bool commit();
};

class VariableContainerNonOwning : public VariableContainer, public MemoryManaged {
private:
    Vector<Variable*> variables;
public:
    VariableContainerNonOwning();

    size_t size() override;
    Variable *getVariable(size_t i) override;
    Variable *getVariable(const ComponentId& component, const char *variableName) override;

    bool add(Variable *variable);
};

class VariableContainerOwning : public VariableContainer, public MemoryManaged {
private:
    Vector<std::unique_ptr<Variable>> variables;
    MO_FilesystemAdapter *filesystem = nullptr;
    char *filename = nullptr;

    uint16_t trackWriteCount = 0;
    bool checkWriteCountUpdated();

    bool loaded = false;

public:
    VariableContainerOwning();
    ~VariableContainerOwning();

    size_t size() override;
    Variable *getVariable(size_t i) override;
    Variable *getVariable(const ComponentId& component, const char *variableName) override;

    bool add(std::unique_ptr<Variable> variable);

    bool enablePersistency(MO_FilesystemAdapter *filesystem, const char *filename); 
    bool load(); //load variables from flash
    bool commit() override;
};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
