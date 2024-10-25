// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#ifndef MO_VARIABLECONTAINER_H
#define MO_VARIABLECONTAINER_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <memory>

#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class VariableContainer {
public:
    ~VariableContainer();
    virtual size_t size() = 0;
    virtual Variable *getVariable(size_t i) = 0;
    virtual Variable *getVariable(const ComponentId& component, const char *variableName) = 0;

    virtual bool commit();
};

class VariableContainerExternal : public VariableContainer, public MemoryManaged {
private:
    Vector<Variable*> variables;
public:
    VariableContainerExternal();

    size_t size() override;
    Variable *getVariable(size_t i) override;
    Variable *getVariable(const ComponentId& component, const char *variableName) override;

    bool add(Variable *variable);
};

class VariableContainerInternal : public VariableContainer, public MemoryManaged {
private:
    Vector<std::unique_ptr<Variable>> variables;
    std::shared_ptr<FilesystemAdapter> filesystem;
    char *filename = nullptr;

    uint16_t trackWriteCount = 0;
    bool checkWriteCountUpdated();

    bool loaded = false;

public:
    VariableContainerInternal();
    ~VariableContainerInternal();

    size_t size() override;
    Variable *getVariable(size_t i) override;
    Variable *getVariable(const ComponentId& component, const char *variableName) override;

    bool add(std::unique_ptr<Variable> variable);

    bool enablePersistency(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename); 
    bool load(); //load variables from flash
    bool commit() override;
};

} //end namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif
