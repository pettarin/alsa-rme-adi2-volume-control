# Changelog

All notable changes to this project will be documented in this file.

> [!NOTE]
> The format is based on
> [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
> and this project adheres to
> [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.0.1] - 2026-11-15

### Added

- Build types `dbg`, `dbg_asan`, `dbg_tsan`, `dbg_ubsan`, and `rls`

### Changed

- Install script `install.sh` now uses the build type `rls`

### Fixed

- A potential heap-buffer-overflow in `strnstr()` own reimplementation
- A pedantic warning about potentially truncated `strncpy`-ed buffers


## [1.0.0] - 2026-11-05

### Added

- Initial release
