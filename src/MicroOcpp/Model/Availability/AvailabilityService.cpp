// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/ChangeAvailability.h>
#include <MicroOcpp/Operations/StatusNotification.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;

v16::AvailabilityServiceEvse::AvailabilityServiceEvse(Context& context, AvailabilityService& availService, unsigned int evseId) : MemoryManaged("v16.Availability.AvailabilityServiceEvse"), context(context), clock(context.getClock()), model(context.getModel16()), availService(availService), evseId(evseId) {

}

v16::AvailabilityServiceEvse::~AvailabilityServiceEvse() {
    if (availabilityBool->getKey() == availabilityBoolKey) {
        availabilityBool->setKey(nullptr);
    }
}


bool v16::AvailabilityServiceEvse::setup() {

    connection = context.getConnection();
    if (!connection) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    snprintf(availabilityBoolKey, sizeof(availabilityBoolKey), MO_CONFIG_EXT_PREFIX "AVAIL_CONN_%d", evseId);
    availabilityBool = configService->declareConfiguration<bool>(availabilityBoolKey, true, MO_KEYVALUE_FN, Mutability::None);
    if (!availabilityBool) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    availabilityContainer = configService->findContainerOfConfiguration(availabilityBool);
    if (!availabilityContainer) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    auto txSvc = context.getModel16().getTransactionService();
    txServiceEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
    if (!txServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    return true;
}

void v16::AvailabilityServiceEvse::loop() {

    if (!trackLoopExecute) {
        trackLoopExecute = true;
    }

    MO_ErrorData errorData;
    mo_ErrorData_init(&errorData);
    errorData.severity = 0;
    int errorDataIndex = -1;

    if (clock.now().isUnixTime()) {

        if (reportedErrorIndex >= 0) {
            auto error = errorDataInputs[reportedErrorIndex].getErrorData(evseId, errorDataInputs[reportedErrorIndex].userData);
            if (error.isError) {
                errorData = error;
                errorDataIndex = reportedErrorIndex;
            }
        }

        for (auto i = std::min(errorDataInputs.size(), trackErrorDataInputs.size()); i >= 1; i--) {
            auto index = i - 1;
            MO_ErrorData error;
            if ((int)index != errorDataIndex) {
                error = errorDataInputs[index].getErrorData(evseId, errorDataInputs[index].userData);
            } else {
                error = errorData;
            }
            if (error.isError && !trackErrorDataInputs[index] && error.severity >= errorData.severity) {
                //new error
                errorData = error;
                errorDataIndex = index;
            } else if (error.isError && error.severity > errorData.severity) {
                errorData = error;
                errorDataIndex = index;
            } else if (!error.isError && trackErrorDataInputs[index]) {
                //reset error
                trackErrorDataInputs[index] = false;
            }
        }

        if (errorDataIndex != reportedErrorIndex) {
            if (errorDataIndex >= 0 || MO_REPORT_NOERROR) {
                reportedStatus = MO_ChargePointStatus_UNDEFINED; //trigger sending currentStatus again with code NoError
            } else {
                reportedErrorIndex = -1;
            }
        }
    }

    auto status = getStatus();

    if (status != currentStatus) {
        MO_DBG_DEBUG("Status changed %s -> %s %s",
                currentStatus == MO_ChargePointStatus_UNDEFINED ? "" : mo_serializeChargePointStatus(currentStatus),
                mo_serializeChargePointStatus(status),
                availService.minimumStatusDurationInt->getInt() ? " (will report delayed)" : "");
        currentStatus = status;
        t_statusTransition = clock.now();
    }

    int32_t dtStatusTransition;
    if (!t_statusTransition.isDefined() || !clock.delta(clock.now(), t_statusTransition, dtStatusTransition)) {
        dtStatusTransition = 0;
    }

    if (reportedStatus != currentStatus &&
            connection->isConnected() &&
            clock.now().isUnixTime() &&
            (availService.minimumStatusDurationInt->getInt() <= 0 || //MinimumStatusDuration disabled
            dtStatusTransition >= availService.minimumStatusDurationInt->getInt())) {
        reportedStatus = currentStatus;
        reportedErrorIndex = errorDataIndex;
        if (errorDataIndex >= 0) {
            trackErrorDataInputs[errorDataIndex] = true;
        }

        auto statusNotificationOp = new StatusNotification(context, evseId, reportedStatus, t_statusTransition, errorData);
        if (!statusNotificationOp) {
            MO_DBG_ERR("OOM");
            return;
        }
        auto statusNotification = makeRequest(context, statusNotificationOp);
        if (!statusNotification) {
            MO_DBG_ERR("OOM");
            return;
        }
        statusNotification->setTimeout(0);
        context.getMessageService().sendRequest(std::move(statusNotification));
        return;
    }
}

void v16::AvailabilityServiceEvse::setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData) {
    this->connectorPluggedInput = connectorPlugged;
    this->connectorPluggedInputUserData = userData;
}

void v16::AvailabilityServiceEvse::setEvReadyInput(bool (*evReady)(unsigned int, void*), void *userData) {
    this->evReadyInput = evReady;
    this->evReadyInputUserData = userData;
}

void v16::AvailabilityServiceEvse::setEvseReadyInput(bool (*evseReady)(unsigned int, void*), void *userData) {
    this->evseReadyInput = evseReady;
    this->evseReadyInputUserData = userData;
}

void v16::AvailabilityServiceEvse::setOccupiedInput(bool (*occupied)(unsigned int, void*), void *userData) {
    this->occupiedInput = occupied;
    this->occupiedInputUserData = userData;
}

bool v16::AvailabilityServiceEvse::addErrorDataInput(MO_ErrorDataInput errorDataInput) {
    size_t capacity = errorDataInputs.size() + 1;
    errorDataInputs.reserve(capacity);
    trackErrorDataInputs.reserve(capacity);
    if (errorDataInputs.capacity() < capacity || trackErrorDataInputs.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }

    errorDataInputs.push_back(errorDataInput);
    trackErrorDataInputs.push_back(false);
    return true;
}

void v16::AvailabilityServiceEvse::setAvailability(bool available) {
    availabilityBool->setBool(available);
    availabilityContainer->commit();
}

void v16::AvailabilityServiceEvse::setAvailabilityVolatile(bool available) {
    availabilityVolatile = available;
}

MO_ChargePointStatus v16::AvailabilityServiceEvse::getStatus() {

    MO_ChargePointStatus res = MO_ChargePointStatus_UNDEFINED;

    /*
    * Handle special case: This is the Connector for the whole CP (i.e. evseId=0) --> only states Available, Unavailable, Faulted are possible
    */
    if (evseId == 0) {
        if (isFaulted()) {
            res = MO_ChargePointStatus_Faulted;
        } else if (!isOperative()) {
            res = MO_ChargePointStatus_Unavailable;
        } else {
            res = MO_ChargePointStatus_Available;
        }
        return res;
    }

    auto transaction = txServiceEvse->getTransaction();

    if (isFaulted()) {
        res = MO_ChargePointStatus_Faulted;
    } else if (!isOperative()) {
        res = MO_ChargePointStatus_Unavailable;
    } else if (transaction && transaction->isRunning()) {
        //Transaction is currently running
        if (connectorPluggedInput && !connectorPluggedInput(evseId, connectorPluggedInputUserData)) { //special case when StopTransactionOnEVSideDisconnect is false
            res = MO_ChargePointStatus_SuspendedEV;
        } else if (!txServiceEvse->ocppPermitsCharge() ||
                (evseReadyInput && !evseReadyInput(evseId, evseReadyInputUserData))) {
            res = MO_ChargePointStatus_SuspendedEVSE;
        } else if (evReadyInput && !evReadyInput(evseId, evReadyInputUserData)) {
            res = MO_ChargePointStatus_SuspendedEV;
        } else {
            res = MO_ChargePointStatus_Charging;
        }
    }
    #if MO_ENABLE_RESERVATION
    else if (model.getReservationService() && model.getReservationService()->getReservation(evseId)) {
        res = MO_ChargePointStatus_Reserved;
    }
    #endif
    else if ((!transaction) &&                                           //no transaction process occupying the connector
               (!connectorPluggedInput || !connectorPluggedInput(evseId, connectorPluggedInputUserData)) &&   //no vehicle plugged
               (!occupiedInput || !occupiedInput(evseId, occupiedInputUserData))) {                       //occupied override clear
        res = MO_ChargePointStatus_Available;
    } else {
        /*
         * Either in Preparing or Finishing state. Only way to know is from previous state
         */
        const auto previous = currentStatus;
        if (previous == MO_ChargePointStatus_Finishing ||
                previous == MO_ChargePointStatus_Charging ||
                previous == MO_ChargePointStatus_SuspendedEV ||
                previous == MO_ChargePointStatus_SuspendedEVSE ||
                (transaction && transaction->getStartSync().isRequested())) { //transaction process still occupying the connector
            res = MO_ChargePointStatus_Finishing;
        } else {
            res = MO_ChargePointStatus_Preparing;
        }
    }

    if (res == MO_ChargePointStatus_UNDEFINED) {
        MO_DBG_DEBUG("status undefined");
        return MO_ChargePointStatus_Faulted; //internal error
    }

    return res;
}

bool v16::AvailabilityServiceEvse::isFaulted() {
    //for (auto i = errorDataInputs.begin(); i != errorDataInputs.end(); ++i) {
    for (size_t i = 0; i < errorDataInputs.size(); i++) {
        if (errorDataInputs[i].getErrorData(evseId, errorDataInputs[i].userData).isFaulted) {
            return true;
        }
    }
    return false;
}

const char *v16::AvailabilityServiceEvse::getErrorCode() {
    if (reportedErrorIndex >= 0) {
        auto error = errorDataInputs[reportedErrorIndex].getErrorData(evseId, errorDataInputs[reportedErrorIndex].userData);
        if (error.isError && error.errorCode) {
            return error.errorCode;
        }
    }
    return nullptr;
}

bool v16::AvailabilityServiceEvse::isOperative() {
    if (isFaulted()) {
        return false;
    }

    if (!trackLoopExecute) {
        return false;
    }

    //check for running transaction(s) - if yes then the connector is always operative
    if (evseId == 0) {
        for (unsigned int cId = 1; cId < model.getNumEvseId(); cId++) {
            auto txService = model.getTransactionService();
            auto txServiceEvse = txService ? txService->getEvse(cId) : nullptr;
            auto transaction = txServiceEvse ? txServiceEvse->getTransaction() : nullptr;
            if (transaction && transaction->isRunning()) {
                return true;
            }
        }
    } else {
        auto transaction = txServiceEvse->getTransaction();
        if (transaction && transaction->isRunning()) {
            return true;
        }
    }

    return availabilityVolatile && availabilityBool->getBool();
}

Operation *v16::AvailabilityServiceEvse::createTriggeredStatusNotification() {

    MO_ErrorData errorData;
    mo_ErrorData_init(&errorData);
    errorData.severity = 0;

    if (reportedErrorIndex >= 0) {
        errorData = errorDataInputs[reportedErrorIndex].getErrorData(evseId, errorDataInputs[reportedErrorIndex].userData);
    } else {
        //find errorData with maximum severity
        for (auto i = errorDataInputs.size(); i >= 1; i--) {
            auto index = i - 1;
            MO_ErrorData error = errorDataInputs[index].getErrorData(evseId, errorDataInputs[index].userData);
            if (error.isError && error.severity >= errorData.severity) {
                errorData = error;
            }
        }
    }

    return new StatusNotification(
                context,
                evseId,
                getStatus(),
                clock.now(),
                errorData);
}

v16::AvailabilityService::AvailabilityService(Context& context) : MemoryManaged("v16.Availability.AvailabilityService"), context(context) {

}

v16::AvailabilityService::~AvailabilityService() {
    for (size_t i = 0; i < MO_NUM_EVSEID && evses[i]; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

bool v16::AvailabilityService::setup() {

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    minimumStatusDurationInt = configService->declareConfiguration<int>("MinimumStatusDuration", 0);
    if (!minimumStatusDurationInt) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    context.getMessageService().registerOperation("ChangeAvailability", [] (Context& context) -> Operation* {
        return new ChangeAvailability(context.getModel16());});

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("StatusNotification", nullptr, nullptr);
    #endif //MO_ENABLE_MOCK_SERVER

    auto rcService = context.getModel16().getRemoteControlService();
    if (!rcService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    rcService->addTriggerMessageHandler("StatusNotification", [] (Context& context, unsigned int evseId) {
        auto availSvc = context.getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        return availSvcEvse ? availSvcEvse->createTriggeredStatusNotification() : nullptr;
    });

    numEvseId = context.getModel16().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("connector init failure");
            return false;
        }
    }

    return true;
}

void v16::AvailabilityService::loop() {
    for (size_t i = 0; i < numEvseId && evses[i]; i++) {
        evses[i]->loop();
    }
}

v16::AvailabilityServiceEvse *v16::AvailabilityService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new AvailabilityServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

using namespace MicroOcpp;

v201::AvailabilityServiceEvse::AvailabilityServiceEvse(Context& context, AvailabilityService& availService, unsigned int evseId) : MemoryManaged("v201.Availability.AvailabilityServiceEvse"), context(context), availService(availService), evseId(evseId), faultedInputs(makeVector<FaultedInput>(getMemoryTag())) {

}

void v201::AvailabilityServiceEvse::loop() {

    if (!trackLoopExecute) {
        trackLoopExecute = true;
    }

    if (evseId >= 1) {
        auto status = getStatus();

        if (status != reportedStatus &&
                context.getClock().now().isUnixTime()) {

            auto statusNotification = makeRequest(context, new StatusNotification(context, evseId, status, context.getClock().now()));
            statusNotification->setTimeout(0);
            context.getMessageService().sendRequest(std::move(statusNotification));
            reportedStatus = status;
            return;
        }
    }
}

void v201::AvailabilityServiceEvse::setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData) {
    this->connectorPluggedInput = connectorPlugged;
    this->connectorPluggedInputUserData = userData;
}

void v201::AvailabilityServiceEvse::setOccupiedInput(bool (*occupied)(unsigned int, void*), void *userData) {
    this->occupiedInput = occupied;
    this->occupiedInputUserData = userData;
}

MO_ChargePointStatus v201::AvailabilityServiceEvse::getStatus() {
    MO_ChargePointStatus res = MO_ChargePointStatus_UNDEFINED;

    if (isFaulted()) {
        res = MO_ChargePointStatus_Faulted;
    } else if (!isAvailable()) {
        res = MO_ChargePointStatus_Unavailable;
    } else if ((!connectorPluggedInput || !connectorPluggedInput(evseId, connectorPluggedInputUserData)) &&   //no vehicle plugged
               (!occupiedInput || !occupiedInput(evseId, occupiedInputUserData))) {                       //occupied override clear
        res = MO_ChargePointStatus_Available;
    } else {
        res = MO_ChargePointStatus_Occupied;
    }

    return res;
}

void v201::AvailabilityServiceEvse::setUnavailable(void *requesterId) {
    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (!unavailableRequesters[i]) {
            unavailableRequesters[i] = requesterId;
            return;
        }
    }
    MO_DBG_ERR("exceeded max. unavailable requesters");
}

void v201::AvailabilityServiceEvse::setAvailable(void *requesterId) {
    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (unavailableRequesters[i] == requesterId) {
            unavailableRequesters[i] = nullptr;
            return;
        }
    }
}

ChangeAvailabilityStatus v201::AvailabilityServiceEvse::changeAvailability(bool operative) {
    if (operative) {
        setAvailable(this);
    } else {
        setUnavailable(this);
    }

    if (!operative) {
        if (isAvailable()) {
            return ChangeAvailabilityStatus::Scheduled;
        }

        if (evseId == 0) {
            for (unsigned int id = 1; id < MO_NUM_EVSEID; id++) {
                if (availService.getEvse(id) && availService.getEvse(id)->isAvailable()) {
                    return ChangeAvailabilityStatus::Scheduled;
                }
            }
        }
    }

    return ChangeAvailabilityStatus::Accepted;
}

Operation *v201::AvailabilityServiceEvse::createTriggeredStatusNotification() {
    return new StatusNotification(
                context,
                evseId,
                getStatus(),
                context.getClock().now());
}

bool v201::AvailabilityServiceEvse::isAvailable() {

    auto txService = context.getModel201().getTransactionService();
    auto txEvse = txService ? txService->getEvse(evseId) : nullptr;
    if (txEvse) {
        if (txEvse->getTransaction() &&
                txEvse->getTransaction()->started &&
                !txEvse->getTransaction()->stopped) {
            return true;
        }
    }

    if (evseId > 0) {
        if (availService.getEvse(0) && !availService.getEvse(0)->isAvailable()) {
            return false;
        }
    }

    for (size_t i = 0; i < MO_INOPERATIVE_REQUESTERS_MAX; i++) {
        if (unavailableRequesters[i]) {
            return false;
        }
    }
    return true;
}

bool v201::AvailabilityServiceEvse::isOperative() {
    if (isFaulted()) {
        return false;
    }

    if (!trackLoopExecute) {
        return false;
    }

    return isAvailable();
}

bool v201::AvailabilityServiceEvse::isFaulted() {
    //for (auto i = errorDataInputs.begin(); i != errorDataInputs.end(); ++i) {
    for (size_t i = 0; i < faultedInputs.size(); i++) {
        if (faultedInputs[i].isFaulted(evseId, faultedInputs[i].userData)) {
            return true;
        }
    }
    return false;
}

bool v201::AvailabilityServiceEvse::addFaultedInput(FaultedInput faultedInput) {
    size_t capacity = faultedInputs.size() + 1;
    faultedInputs.reserve(capacity);
    if (faultedInputs.capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return false;
    }
    faultedInputs.push_back(faultedInput);
    return true;
}

v201::AvailabilityService::AvailabilityService(Context& context) : MemoryManaged("v201.Availability.AvailabilityService"), context(context) {

}

v201::AvailabilityService::~AvailabilityService() {
    for (size_t i = 0; i < MO_NUM_EVSEID && evses[i]; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

bool v201::AvailabilityService::setup() {

    auto varSvc = context.getModel201().getVariableService();
    if (!varSvc) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    offlineThreshold = varSvc->declareVariable<int>("OCPPCommCtrlr", "OfflineThreshold", 600);
    if (!offlineThreshold) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    context.getMessageService().registerOperation("ChangeAvailability", [] (Context& context) -> Operation* {
        return new ChangeAvailability(*context.getModel201().getAvailabilityService());});

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("StatusNotification", nullptr, nullptr);
    #endif //MO_ENABLE_MOCK_SERVER

    auto rcService = context.getModel201().getRemoteControlService();
    if (!rcService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    rcService->addTriggerMessageHandler("StatusNotification", [] (Context& context, unsigned int evseId) {
        auto availSvc = context.getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        return availSvcEvse ? availSvcEvse->createTriggeredStatusNotification() : nullptr;
    });

    numEvseId = context.getModel201().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i)) {
            MO_DBG_ERR("connector init failure");
            return false;
        }
    }

    return true;
}

void v201::AvailabilityService::loop() {
    for (size_t i = 0; i < numEvseId && evses[i]; i++) {
        evses[i]->loop();
    }
}

v201::AvailabilityServiceEvse *v201::AvailabilityService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new AvailabilityServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

#endif //MO_ENABLE_V201
