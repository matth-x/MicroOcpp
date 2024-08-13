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

#include <vector>
#include <memory>

#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class VariableContainer {
private:
    const char *filename;
    bool accessible;
public:
    VariableContainer(const char *filename, bool accessible) : filename(filename), accessible(accessible) { }

    virtual ~VariableContainer();

    const char *getFilename() {return filename;}
    bool isAccessible() const {return accessible;} //accessible by OCPP server or only used as internal persistent store

    virtual bool load() = 0; //called at the end of mocpp_intialize, to load the Variables with the stored value
    virtual bool save() = 0;

    /*
     * Factory method to create Variable objects. This doesn't add the returned Variable to the managed
     * variable store. Instead, the caller must add the returned Variable via `add(...)`
     * The function signature consists of the requested low-level data type (Int, Bool, or String) and
     * a composite key to identify the Variable to create (componentName x variableName x attribute type)
     *
     * Variable::InternalDataType dtype: internal low-level data type. Defines which value getters / setters are valid.
     *         if dtype == InternalDataType::Int, then getInt() and setInt(...) are valid
     *         if dtype == InternalDataType::String, then getString() and setString(...) are valid. Etc.
     */
    virtual std::unique_ptr<Variable> createVariable(Variable::InternalDataType dtype, Variable::AttributeTypeSet attributes) = 0; // factory method
    virtual bool add(std::unique_ptr<Variable> variable) = 0;

    virtual size_t size() const = 0;
    virtual Variable *getVariable(size_t i) const = 0;
    virtual Variable *getVariable(const ComponentId& component, const char *variableName) const = 0;
};

class VariableContainerVolatile : public VariableContainer, public MemoryManaged {
private:
    Vector<std::unique_ptr<Variable>> variables;
public:
    VariableContainerVolatile(const char *filename, bool accessible);
    ~VariableContainerVolatile();

    //VariableContainer definitions
    bool load() override;
    bool save() override;
    std::unique_ptr<Variable> createVariable(Variable::InternalDataType dtype, Variable::AttributeTypeSet attributes) override;
    bool add(std::unique_ptr<Variable> config) override;
    size_t size() const override;
    Variable *getVariable(size_t i) const override;
    Variable *getVariable(const ComponentId& component, const char *variableName) const override;
};

std::unique_ptr<VariableContainerVolatile> makeVariableContainerVolatile(const char *filename, bool accessible);

} //end namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif
