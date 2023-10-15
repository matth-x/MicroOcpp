// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <string>

#include <MicroOcpp/Model/ConnectorBase/ConnectorsCommon.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Operations/ChangeAvailability.h>
#include <MicroOcpp/Operations/ChangeConfiguration.h>
#include <MicroOcpp/Operations/ClearCache.h>
#include <MicroOcpp/Operations/GetConfiguration.h>
#include <MicroOcpp/Operations/RemoteStartTransaction.h>
#include <MicroOcpp/Operations/RemoteStopTransaction.h>
#include <MicroOcpp/Operations/Reset.h>
#include <MicroOcpp/Operations/TriggerMessage.h>
#include <MicroOcpp/Operations/UnlockConnector.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

ConnectorsCommon::ConnectorsCommon(Context& context, unsigned int numConn, std::shared_ptr<FilesystemAdapter> filesystem) :
        context(context) {
    
    declareConfiguration<int>("NumberOfConnectors", numConn >= 1 ? numConn - 1 : 0, CONFIGURATION_VOLATILE, true);
    
    /*
     * Further configuration keys which correspond to the Core profile
     */
    declareConfiguration<bool>("AuthorizeRemoteTxRequests", false, CONFIGURATION_VOLATILE, true);
    declareConfiguration<int>("GetConfigurationMaxKeys", 30, CONFIGURATION_VOLATILE, true);
    
    context.getOperationRegistry().registerOperation("ChangeAvailability", [&context] () {
        return new Ocpp16::ChangeAvailability(context.getModel());});
    context.getOperationRegistry().registerOperation("ChangeConfiguration", [] () {
        return new Ocpp16::ChangeConfiguration();});
    context.getOperationRegistry().registerOperation("ClearCache", [filesystem] () {
        return new Ocpp16::ClearCache(filesystem);});
    context.getOperationRegistry().registerOperation("GetConfiguration", [] () {
        return new Ocpp16::GetConfiguration();});
    context.getOperationRegistry().registerOperation("RemoteStartTransaction", [&context] () {
        return new Ocpp16::RemoteStartTransaction(context.getModel());});
    context.getOperationRegistry().registerOperation("RemoteStopTransaction", [&context] () {
        return new Ocpp16::RemoteStopTransaction(context.getModel());});
    context.getOperationRegistry().registerOperation("Reset", [&context] () {
        return new Ocpp16::Reset(context.getModel());});
    context.getOperationRegistry().registerOperation("TriggerMessage", [&context] () {
        return new Ocpp16::TriggerMessage(context);});
    context.getOperationRegistry().registerOperation("UnlockConnector", [&context] () {
        return new Ocpp16::UnlockConnector(context.getModel());});

    /*
     * Register further message handlers to support echo mode: when this library
     * is connected with a WebSocket echo server, let it reply to its own requests.
     * Mocking an OCPP Server on the same device makes running (unit) tests easier.
     */
    context.getOperationRegistry().registerOperation("Authorize", [&context] () {
        return new Ocpp16::Authorize(context.getModel(), "");});
    context.getOperationRegistry().registerOperation("StartTransaction", [&context] () {
        return new Ocpp16::StartTransaction(context.getModel(), nullptr);});
    context.getOperationRegistry().registerOperation("StatusNotification", [&context] () {
        return new Ocpp16::StatusNotification(-1, ChargePointStatus::NOT_SET, Timestamp());});
    context.getOperationRegistry().registerOperation("StopTransaction", [&context] () {
        return new Ocpp16::StopTransaction(context.getModel(), nullptr);});
}

void ConnectorsCommon::loop() {
    //do nothing
}
