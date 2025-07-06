// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

/*
 * Implementation of the UCs B05 - B06
 */

#include <string.h>

#include <MicroOcpp/Model/Variables/VariableContainer.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::v201;

VariableContainer::~VariableContainer() {

}

bool VariableContainer::commit() {
    return true;
}

VariableContainerNonOwning::VariableContainerNonOwning() :
            VariableContainer(), MemoryManaged("v201.Variables.VariableContainerNonOwning"), variables(makeVector<Variable*>(getMemoryTag())) {

}

size_t VariableContainerNonOwning::size() {
    return variables.size();
}

Variable *VariableContainerNonOwning::getVariable(size_t i) {
    return variables[i];
}

Variable *VariableContainerNonOwning::getVariable(const ComponentId& component, const char *variableName) {
    for (size_t i = 0; i < variables.size(); i++) {
        auto& var = variables[i];
        if (!strcmp(var->getName(), variableName) &&
                var->getComponentId().equals(component)) {
            return var;
        }
    }
    return nullptr;
}

bool VariableContainerNonOwning::add(Variable *variable) {
    variables.push_back(variable);
    return true;
}

bool VariableContainerOwning::checkWriteCountUpdated() {

    decltype(trackWriteCount) writeCount = 0;

    for (size_t i = 0; i < variables.size(); i++) {
        writeCount += variables[i]->getWriteCount();
    }

    bool updated = writeCount != trackWriteCount;

    trackWriteCount = writeCount;

    return updated;
}

VariableContainerOwning::VariableContainerOwning() :
            VariableContainer(), MemoryManaged("v201.Variables.VariableContainerOwning"), variables(makeVector<std::unique_ptr<Variable>>(getMemoryTag())) {

}

VariableContainerOwning::~VariableContainerOwning() {
    MO_FREE(filename);
    filename = nullptr;
}

size_t VariableContainerOwning::size() {
    return variables.size();
}

Variable *VariableContainerOwning::getVariable(size_t i) {
    return variables[i].get();
}

Variable *VariableContainerOwning::getVariable(const ComponentId& component, const char *variableName) {
    for (size_t i = 0; i < variables.size(); i++) {
        auto& var = variables[i];
        if (!strcmp(var->getName(), variableName) &&
                var->getComponentId().equals(component)) {
            return var.get();
        }
    }
    return nullptr;
}

bool VariableContainerOwning::add(std::unique_ptr<Variable> variable) {
    variables.push_back(std::move(variable));
    return true;
}

bool VariableContainerOwning::enablePersistency(MO_FilesystemAdapter *filesystem, const char *filename) {
    this->filesystem = filesystem;

    MO_FREE(this->filename);
    this->filename = nullptr;

    size_t fnsize = strlen(filename) + 1;

    this->filename = static_cast<char*>(MO_MALLOC(getMemoryTag(), fnsize));
    if (!this->filename) {
        MO_DBG_ERR("OOM");
        return false;
    }

    snprintf(this->filename, fnsize, "%s", filename);
    return true;
}

bool VariableContainerOwning::load() {
    if (loaded) {
        return true;
    }

    if (!filesystem || !filename) {
        return true; //persistency disabled - nothing to do
    }

    JsonDoc doc (0);
    auto ret = FilesystemUtils::loadJson(filesystem, filename, doc, getMemoryTag());
    switch (ret) {
        case FilesystemUtils::LoadStatus::Success:
            break; //continue loading JSON
        case FilesystemUtils::LoadStatus::FileNotFound:
            MO_DBG_DEBUG("Populate FS: create variables file");
            return commit();
        case FilesystemUtils::LoadStatus::ErrOOM:
            MO_DBG_ERR("OOM");
            return false;
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("failed to load %s", filename);
            return false;
    }

    JsonArray variablesJson = doc["variables"];

    for (JsonObject stored : variablesJson) {

        const char *component = stored["component"] | (const char*)nullptr;
        int evseId = stored["evseId"] | -1;
        const char *name = stored["name"] | (const char*)nullptr;

        if (!component || !name) {
            MO_DBG_ERR("corrupt entry: %s", filename);
            continue;
        }

        auto variablePtr = getVariable(ComponentId(component, EvseId(evseId)), name);
        if (!variablePtr) {
            MO_DBG_ERR("loaded variable does not exist: %s, %s, %s", filename, component, name);
            continue;
        }

        auto& variable = *variablePtr;

        switch (variable.getInternalDataType()) {
            case Variable::InternalDataType::Int:
                if (variable.hasAttribute(Variable::AttributeType::Actual)) variable.setInt(stored["valActual"] | 0, Variable::AttributeType::Actual);
                if (variable.hasAttribute(Variable::AttributeType::Target)) variable.setInt(stored["valTarget"] | 0, Variable::AttributeType::Target);
                if (variable.hasAttribute(Variable::AttributeType::MinSet)) variable.setInt(stored["valMinSet"] | 0, Variable::AttributeType::MinSet);
                if (variable.hasAttribute(Variable::AttributeType::MaxSet)) variable.setInt(stored["valMaxSet"] | 0, Variable::AttributeType::MaxSet);
                break;
            case Variable::InternalDataType::Bool:
                if (variable.hasAttribute(Variable::AttributeType::Actual)) variable.setBool(stored["valActual"] | false, Variable::AttributeType::Actual);
                if (variable.hasAttribute(Variable::AttributeType::Target)) variable.setBool(stored["valTarget"] | false, Variable::AttributeType::Target);
                if (variable.hasAttribute(Variable::AttributeType::MinSet)) variable.setBool(stored["valMinSet"] | false, Variable::AttributeType::MinSet);
                if (variable.hasAttribute(Variable::AttributeType::MaxSet)) variable.setBool(stored["valMaxSet"] | false, Variable::AttributeType::MaxSet);
                break;
            case Variable::InternalDataType::String:
                bool success = true;
                if (variable.hasAttribute(Variable::AttributeType::Actual)) success &= variable.setString(stored["valActual"] | "", Variable::AttributeType::Actual);
                if (variable.hasAttribute(Variable::AttributeType::Target)) success &= variable.setString(stored["valTarget"] | "", Variable::AttributeType::Target);
                if (variable.hasAttribute(Variable::AttributeType::MinSet)) success &= variable.setString(stored["valMinSet"] | "", Variable::AttributeType::MinSet);
                if (variable.hasAttribute(Variable::AttributeType::MaxSet)) success &= variable.setString(stored["valMaxSet"] | "", Variable::AttributeType::MaxSet);
                if (!success) {
                    MO_DBG_ERR("value error: %s, %s, %s", filename, component, name);
                    continue;
                }
                break;
        }
    }

    checkWriteCountUpdated(); // update trackWriteCount after load is completed

    MO_DBG_DEBUG("Initialization finished");
    loaded = true;
    return true;
}

bool VariableContainerOwning::commit() {
    if (!filesystem || !filename) {
        //persistency disabled - nothing to do
        return true;
    }

    if (!checkWriteCountUpdated()) {
        return true; //nothing to be done
    }

    size_t jsonCapacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(0);
    size_t variableCapacity = 0;
    for (size_t i = 0; i < variables.size(); i++) {
        auto& variable = *variables[i];

        if (!variable.isPersistent()) {
            continue;
        }

        size_t addedJsonCapacity = JSON_ARRAY_SIZE(variableCapacity + 1) - JSON_ARRAY_SIZE(variableCapacity);

        size_t storedEntities = 2; //component name, variable name will always be stored
        storedEntities += variable.getComponentId().evse.id >= 0 ?                 1 : 0;
        storedEntities += variable.hasAttribute(Variable::AttributeType::Actual) ? 1 : 0;
        storedEntities += variable.hasAttribute(Variable::AttributeType::Target) ? 1 : 0;
        storedEntities += variable.hasAttribute(Variable::AttributeType::MinSet) ? 1 : 0;
        storedEntities += variable.hasAttribute(Variable::AttributeType::MaxSet) ? 1 : 0;

        addedJsonCapacity += JSON_OBJECT_SIZE(storedEntities);

        if (jsonCapacity + addedJsonCapacity <= MO_MAX_JSON_CAPACITY) {
            jsonCapacity += addedJsonCapacity;
            variableCapacity++;
        } else {
            MO_DBG_ERR("configs JSON exceeds maximum capacity (%s, %zu entries). Crop configs file (by FCFS)", filename, variables.size());
            break;
        }
    }

    auto doc = initJsonDoc(getMemoryTag(), jsonCapacity);

    JsonArray variablesJson = doc.createNestedArray("variables");

    for (size_t i = 0; i < variableCapacity; i++) {
        auto& variable = *variables[i];

        if (!variable.isPersistent()) {
            continue;
        }

        auto stored = variablesJson.createNestedObject();

        stored["component"] = variable.getComponentId().name;
        if (variable.getComponentId().evse.id >= 0) {
            stored["evseId"] = variable.getComponentId().evse.id;
        }
        stored["name"] = variable.getName();

        switch (variable.getInternalDataType()) {
            case Variable::InternalDataType::Int:
                if (variable.hasAttribute(Variable::AttributeType::Actual)) stored["valActual"] = variable.getInt(Variable::AttributeType::Actual);
                if (variable.hasAttribute(Variable::AttributeType::Target)) stored["valTarget"] = variable.getInt(Variable::AttributeType::Target);
                if (variable.hasAttribute(Variable::AttributeType::MinSet)) stored["valMinSet"] = variable.getInt(Variable::AttributeType::MinSet);
                if (variable.hasAttribute(Variable::AttributeType::MaxSet)) stored["valMaxSet"] = variable.getInt(Variable::AttributeType::MaxSet);
                break;
            case Variable::InternalDataType::Bool:
                if (variable.hasAttribute(Variable::AttributeType::Actual)) stored["valActual"] = variable.getBool(Variable::AttributeType::Actual);
                if (variable.hasAttribute(Variable::AttributeType::Target)) stored["valTarget"] = variable.getBool(Variable::AttributeType::Target);
                if (variable.hasAttribute(Variable::AttributeType::MinSet)) stored["valMinSet"] = variable.getBool(Variable::AttributeType::MinSet);
                if (variable.hasAttribute(Variable::AttributeType::MaxSet)) stored["valMaxSet"] = variable.getBool(Variable::AttributeType::MaxSet);
                break;
            case Variable::InternalDataType::String:
                if (variable.hasAttribute(Variable::AttributeType::Actual)) stored["valActual"] = variable.getString(Variable::AttributeType::Actual);
                if (variable.hasAttribute(Variable::AttributeType::Target)) stored["valTarget"] = variable.getString(Variable::AttributeType::Target);
                if (variable.hasAttribute(Variable::AttributeType::MinSet)) stored["valMinSet"] = variable.getString(Variable::AttributeType::MinSet);
                if (variable.hasAttribute(Variable::AttributeType::MaxSet)) stored["valMaxSet"] = variable.getString(Variable::AttributeType::MaxSet);
                break;
        }
    }

    auto ret = FilesystemUtils::storeJson(filesystem, filename, doc);
    bool success = (ret == FilesystemUtils::StoreStatus::Success);

    if (success) {
        MO_DBG_DEBUG("Saving variables finished");
    } else {
        MO_DBG_ERR("could not save variables file: %s", filename);
    }

    return success;
}

#endif //MO_ENABLE_V201
