# Changelog

## Unreleased

### Changed

- Improved UUID generation ([#383](https://github.com/matth-x/MicroOcpp/pull/383))

### Fixed

- Timing issues for OCTT test cases ([#383](https://github.com/matth-x/MicroOcpp/pull/383))
- Misleading Reset failure dbg msg ([#388](https://github.com/matth-x/MicroOcpp/pull/388))

## [1.2.0] - 2024-11-03

### Changed

- Change `MicroOcpp::ChargePointStatus` into C-style enum ([#309](https://github.com/matth-x/MicroOcpp/pull/309))
- Connector lock disabled by default per `MO_ENABLE_CONNECTOR_LOCK` ([#312](https://github.com/matth-x/MicroOcpp/pull/312))
- Relaxed temporal order of non-tx-related operations ([#345](https://github.com/matth-x/MicroOcpp/pull/345))
- Use pseudo-GUIDs as messageId ([#345](https://github.com/matth-x/MicroOcpp/pull/345))
- ISO 8601 milliseconds omitted by default ([352](https://github.com/matth-x/MicroOcpp/pull/352))
- Rename `MO_NUM_EVSE` into `MO_NUM_EVSEID` (v2.0.1) ([#371](https://github.com/matth-x/MicroOcpp/pull/371))
- Change `MicroOcpp::ReadingContext` into C-style struct ([#371](https://github.com/matth-x/MicroOcpp/pull/371))
- Refactor RequestStartTransaction (v2.0.1) ([#371](https://github.com/matth-x/MicroOcpp/pull/371))

### Added

- Provide ChargePointStatus in API ([#309](https://github.com/matth-x/MicroOcpp/pull/309))
- Built-in OTA over FTP ([#313](https://github.com/matth-x/MicroOcpp/pull/313))
- Built-in Diagnostics over FTP ([#313](https://github.com/matth-x/MicroOcpp/pull/313))
- Error `severity` mechanism ([#331](https://github.com/matth-x/MicroOcpp/pull/331))
- Build flag `MO_REPORT_NOERROR` to report error recovery ([#331](https://github.com/matth-x/MicroOcpp/pull/331))
- Support for `parentIdTag` ([#344](https://github.com/matth-x/MicroOcpp/pull/344))
- Input validation for unsigned int Configs ([#344](https://github.com/matth-x/MicroOcpp/pull/344))
- Support for TransactionMessageAttempts/-RetryInterval ([#345](https://github.com/matth-x/MicroOcpp/pull/345), [#380](https://github.com/matth-x/MicroOcpp/pull/380))
- Heap profiler and custom allocator support ([#350](https://github.com/matth-x/MicroOcpp/pull/350))
- Migration of persistent storage ([#355](https://github.com/matth-x/MicroOcpp/pull/355))
- Benchmarks pipeline ([#369](https://github.com/matth-x/MicroOcpp/pull/369), [#376](https://github.com/matth-x/MicroOcpp/pull/376))
- MeterValues port for OCPP 2.0.1 ([#371](https://github.com/matth-x/MicroOcpp/pull/371))
- UnlockConnector port for OCPP 2.0.1 ([#371](https://github.com/matth-x/MicroOcpp/pull/371))
- More APIs ported to OCPP 2.0.1 ([#371](https://github.com/matth-x/MicroOcpp/pull/371))
- Support for AuthorizeRemoteTxRequests ([#373](https://github.com/matth-x/MicroOcpp/pull/373))
- Persistent Variable and Tx store for OCPP 2.0.1 ([#379](https://github.com/matth-x/MicroOcpp/pull/379))

### Removed

- ESP32 built-in HTTP OTA ([#313](https://github.com/matth-x/MicroOcpp/pull/313))
- Operation store (files op-*.jsn and opstore.jsn) ([#345](https://github.com/matth-x/MicroOcpp/pull/345))
- Explicit tracking of txNr (file txstore.jsn) ([#345](https://github.com/matth-x/MicroOcpp/pull/345))
- SimpleRequestFactory ([#351](https://github.com/matth-x/MicroOcpp/pull/351))

### Fixed

- Skip Unix files . and .. in ftw_root ([#313](https://github.com/matth-x/MicroOcpp/pull/313))
- Skip clock-aligned measurements when time not set
- Hold back error StatusNotifs when time not set ([#311](https://github.com/matth-x/MicroOcpp/issues/311))
- Don't send Available when tx occupies connector ([#315](https://github.com/matth-x/MicroOcpp/issues/315))
- Make ChargingScheduleAllowedChargingRateUnit read-only ([#328](https://github.com/matth-x/MicroOcpp/issues/328))
- ~Don't send StatusNotifs while offline ([#344](https://github.com/matth-x/MicroOcpp/pull/344))~ (see ([#371](https://github.com/matth-x/MicroOcpp/pull/371)))
- Don't change into Unavailable upon Reset ([#344](https://github.com/matth-x/MicroOcpp/pull/344))
- Reject DataTransfer by default ([#344](https://github.com/matth-x/MicroOcpp/pull/344))
- UnlockConnector NotSupported if connectorId invalid ([#344](https://github.com/matth-x/MicroOcpp/pull/344))
- Fix regression bug of [#345](https://github.com/matth-x/MicroOcpp/pull/345) ([#353](https://github.com/matth-x/MicroOcpp/pull/353), [#356](https://github.com/matth-x/MicroOcpp/pull/356))
- Correct MeterValue PreBoot timestamp ([#354](https://github.com/matth-x/MicroOcpp/pull/354))
- Send errorCode in triggered StatusNotif ([#359](https://github.com/matth-x/MicroOcpp/pull/359))
- Remove int to bool conversion in ChangeConfig ([#362](https://github.com/matth-x/MicroOcpp/pull/362))
- Multiple fixes of the OCPP 2.0.1 extension ([#371](https://github.com/matth-x/MicroOcpp/pull/371))

## [1.1.0] - 2024-05-21

### Changed

- Replace `PollResult<bool>` with enum `UnlockConnectorResult` ([#271](https://github.com/matth-x/MicroOcpp/pull/271))
- Rename master branch into main
- Tx logic directly checks if WebSocket is offline ([#282](https://github.com/matth-x/MicroOcpp/pull/282))
- `ocppPermitsCharge` ignores Faulted state ([#279](https://github.com/matth-x/MicroOcpp/pull/279))
- `setEnergyMeterInput` expects `int` input ([#301](https://github.com/matth-x/MicroOcpp/pull/301))

### Added

- File index ([#270](https://github.com/matth-x/MicroOcpp/pull/270))
- Config `Cst_TxStartOnPowerPathClosed` to put back TxStartPoint ([#271](https://github.com/matth-x/MicroOcpp/pull/271))
- Build flag `MO_ENABLE_RESERVATION=0` disables Reservation module ([#302](https://github.com/matth-x/MicroOcpp/pull/302))
- Build flag `MO_ENABLE_LOCAL_AUTH=0` disables LocalAuthList module ([#303](https://github.com/matth-x/MicroOcpp/pull/303))
- Function `bool isConnected()` in `Connection` interface ([#282](https://github.com/matth-x/MicroOcpp/pull/282))
- Build flags for customizing memory limits of SmartCharging ([#260](https://github.com/matth-x/MicroOcpp/pull/260))
- SConscript ([#287](https://github.com/matth-x/MicroOcpp/pull/287))
- C-API for custom Configs store ([297](https://github.com/matth-x/MicroOcpp/pull/297))
- Certificate Management, UCs M03 - M05 ([#262](https://github.com/matth-x/MicroOcpp/pull/262), [#274](https://github.com/matth-x/MicroOcpp/pull/274), [#292](https://github.com/matth-x/MicroOcpp/pull/292))
- FTP Client ([#291](https://github.com/matth-x/MicroOcpp/pull/291))
- `ProtocolVersion` selects v1.6 or v2.0.1 ([#247](https://github.com/matth-x/MicroOcpp/pull/247))
- Build flag `MO_ENABLE_V201=1` enables OCPP 2.0.1 features ([#247](https://github.com/matth-x/MicroOcpp/pull/247))
    - Variables (non-persistent), UCs B05 - B07 ([#247](https://github.com/matth-x/MicroOcpp/pull/247), [#284](https://github.com/matth-x/MicroOcpp/pull/284))
    - Transactions (preview only), UCs E01 - E12 ([#247](https://github.com/matth-x/MicroOcpp/pull/247))
    - StatusNotification compatibility ([#247](https://github.com/matth-x/MicroOcpp/pull/247))
    - ChangeAvailability compatibility ([#285](https://github.com/matth-x/MicroOcpp/pull/285))
    - Reset compatibility, UCs B11 - B12 ([#286](https://github.com/matth-x/MicroOcpp/pull/286))
    - RequestStart-/StopTransaction, UCs F01 - F02 ([#289](https://github.com/matth-x/MicroOcpp/pull/289))

### Fixed

- Fix defect idTag check in `endTransaction` ([#275](https://github.com/matth-x/MicroOcpp/pull/275))
- Make field localAuthorizationList in SendLocalList optional
- Update charging profiles when flash disabled (relates to [#260](https://github.com/matth-x/MicroOcpp/pull/260))
- Ignore UnlockConnector when handler not set ([#271](https://github.com/matth-x/MicroOcpp/pull/271))
- Reject ChargingProfile if unit not supported ([#271](https://github.com/matth-x/MicroOcpp/pull/271))
- Fix building with debug level warn and error
- Reduce debug output FW size overhead ([#304](https://github.com/matth-x/MicroOcpp/pull/304))
- Fix transaction freeze in offline mode ([#279](https://github.com/matth-x/MicroOcpp/pull/279), [#287](https://github.com/matth-x/MicroOcpp/pull/287))
- Fix compilation error caused by `PRId32` ([#279](https://github.com/matth-x/MicroOcpp/pull/279))
- Don't load FW-mngt. module when no handlers set ([#271](https://github.com/matth-x/MicroOcpp/pull/271))
- Change arduinoWebSockets URL param to path ([#278](https://github.com/matth-x/MicroOcpp/issues/278))
- Avoid creating conf when operation fails ([#290](https://github.com/matth-x/MicroOcpp/pull/290))
- Fix whitespaces in MeterValues ([#301](https://github.com/matth-x/MicroOcpp/pull/301))
- Make SmartChargingProfile txId field optional ([#348](https://github.com/matth-x/MicroOcpp/pull/348))

## [1.0.3] - 2024-04-06

### Fixed

- Fix nullptr access in endTransaction ([#275](https://github.com/matth-x/MicroOcpp/pull/275))
- Backport: Fix building with debug level warn and error

## [1.0.2] - 2024-03-24

### Fixed

- Correct MO version numbers in code (they were still `1.0.0`)

## [1.0.1] - 2024-02-27

### Fixed

- Allow `nullptr` as parameter for `mocpp_set_console_out` ([#224](https://github.com/matth-x/MicroOcpp/issues/224))
- Fix `mocpp_tick_ms()` on esp-idf roll-over after 12 hours
- Pin ArduinoJson to v6.21 ([#245](https://github.com/matth-x/MicroOcpp/issues/245))
- Fix bounds checking in SmartCharging module ([#260](https://github.com/matth-x/MicroOcpp/pull/260))

## [1.0.0] - 2023-10-22

_First release_

### Changed

- `mocpp_initialize` takes OCPP URL without explicit host, port ([#220](https://github.com/matth-x/MicroOcpp/pull/220))
- `endTransaction` checks authorization of `idTag`
- Update configurations API ([#195](https://github.com/matth-x/MicroOcpp/pull/195))
- Update Firmware- and DiagnosticsService API ([#207](https://github.com/matth-x/MicroOcpp/pull/207))
- Update Connection interface
- Update Authorization module functions ([#213](https://github.com/matth-x/MicroOcpp/pull/213))
- Reflect changes in C-API
- Change build flag prefix from `MOCPP_` to `MO_`
- Change `mo_set_console_out` to `mocpp_set_console_out`
- Revise README.md
- Revise misleading debug messages
- Update Arduino IDE manifest ([#206](https://github.com/matth-x/MicroOcpp/issues/206))

### Added

- Auto-recovery switch in `mocpp_initialize` params
- WebAssembly port
- Configurable `MO_PARTITION_LABEL` for the esp-idf SPIFFS integration ([#218](https://github.com/matth-x/MicroOcpp/pull/218))
- `MO_TX_CLEAN_ABORTED=0` keeps aborted txs in journal
- `MO_VERSION` specifier
- `MO_PLATFORM_NONE` for compilation on custom platforms
- `endTransaction_authorized` enforces the tx end
- Add valgrind, ASan, UBSan CI/CD steps ([#189](https://github.com/matth-x/MicroOcpp/pull/189))

### Fixed

- Reservation ([#196](https://github.com/matth-x/MicroOcpp/pull/196))
- Fix immediate FW-update Download phase abort ([#216](https://github.com/matth-x/MicroOcpp/pull/216))
- `stat` usage on arduino-esp32 LittleFS
- SetChargingProfile JSON capacity calculation
- Set correct idTag when Reset triggers StopTx
- Execute operations only once despite multiple .conf send attempts ([#207](https://github.com/matth-x/MicroOcpp/pull/207))
- ConnectionTimeOut only applies when connector is still unplugged
- Fix valgrind warnings

## [1eff6e5] - 23-08-23

_Previous point with breaking changes on master_

Renaming to MicroOcpp is completed since this commit. See the [migration guide](https://matth-x.github.io/MicroOcpp/migration/) for more details on what's changed. Changelogs and semantic versioning are adopted starting with v1.0.0

## [0.3.0] - 23-08-19

_Last version under the project name ArduinoOcpp_
