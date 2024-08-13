// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Variables/VariableContainer.h>

#include <string.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

VariableContainer::~VariableContainer() {

}

VariableContainerVolatile::VariableContainerVolatile(const char *filename, bool accessible) :
        VariableContainer(filename, accessible), MemoryManaged("v201.Variables.VariableContainerVolatile.", filename), variables(makeVector<std::unique_ptr<Variable>>(getMemoryTag())) {

}

VariableContainerVolatile::~VariableContainerVolatile() {

}

bool VariableContainerVolatile::load() {
    return true;
}

bool VariableContainerVolatile::save() {
    return true;
}

std::unique_ptr<Variable> VariableContainerVolatile::createVariable(Variable::InternalDataType dtype, Variable::AttributeTypeSet attributes) {
    return makeVariable(dtype, attributes);
}

bool VariableContainerVolatile::add(std::unique_ptr<Variable> variable) {
    variables.push_back(std::move(variable));
    return true;
}

size_t VariableContainerVolatile::size() const {
    return variables.size();
}

Variable *VariableContainerVolatile::getVariable(size_t i) const {
    return variables[i].get();
}

Variable *VariableContainerVolatile::getVariable(const ComponentId& component, const char *variableName) const {
    for (auto it = variables.begin(); it != variables.end(); it++) {
        if (!strcmp((*it)->getName(), variableName) &&
                (*it)->getComponentId().equals(component)) {
            return it->get();
        }
    }
    return nullptr;
}

std::unique_ptr<VariableContainerVolatile> MicroOcpp::makeVariableContainerVolatile(const char *filename, bool accessible) {
    return std::unique_ptr<VariableContainerVolatile>(new VariableContainerVolatile(filename, accessible));
}

#endif // MO_ENABLE_V201
