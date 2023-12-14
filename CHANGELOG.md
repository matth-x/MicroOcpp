# Changelog

## [Unreleased]

### Fixed

- Allow `nullptr` as parameter for `mocpp_set_console_out` ([#224](https://github.com/matth-x/MicroOcpp/issues/224))
- Fix `mocpp_tick_ms()` on esp-idf roll-over after 12 hours

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
