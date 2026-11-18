# Changelog

All notable changes to this project will be documented in this file.

> [!NOTE]
> The format is based on
> [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
> and this project adheres to
> [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.0.2] - 2026-11-18

### Changed

- Default value of --phones34-min-db option to `-90.0`
- Default value of --phones34-max-db option to `-40.0`
- Several cosmetic code improvements

### Fixed

- ADI-2 DAC devices were not autodetected
- Option `--card-number` was ignored when `--device-name` was specified
  and `--device-id` was not specified
- Some minor issues in error and cleanup execution paths, and logging


## [1.0.1] - 2026-11-15

### Added

- Build types `dbg`, `dbg_asan`, `dbg_tsan`, `dbg_ubsan`, and `rls`
- Option `--exit-codes` to print the exit codes

### Changed

- Install script `install.sh` to use the build type `rls`
- Service exit codes to be aligned to systemd/LSB semantics

### Fixed

- A potential heap-buffer-overflow in `strnstr()` own reimplementation
- A pedantic warning about potentially truncated `strncpy`-ed buffers


## [1.0.0] - 2026-11-05

### Added

- Initial release
