# Tested Hardware/Software Configurations

The table below reports the hardware/software configurations
that have been tested working (or not):

| RME ADI-2 Device | Architecture | OS         | OS Version | ALSA Version     | Service Version | Working?           | Date       | Tester    | Notes                |
| ---------------- | ------------ | ---------- | ---------- | ---------------- | --------------- | ------------------ | ---------- | --------- | -------------------- |
| ADI-2/4 Pro SE   | `x86_64`     | Debian     | `testing ` | `k6.1.0-3-amd64` | `1.0.0`         | :white_check_mark: | 2026-11-05 | @pettarin | Lenovo ThinkPad X260 |
| ADI-2/4 Pro SE   | `x86_64`     | Volumio OS | `4.239`    |                  | `1.0.0`         | :white_check_mark: | 2026-11-05 | @pettarin | VM in Proxmox VM     |
| ADI-2/4 Pro SE   | `armhf`      | Volumio OS | `4.239`    |                  | `1.0.0`         | :white_check_mark: | 2026-11-05 | @pettarin | Raspberry Pi 3B+     |
| ADI-2/4 Pro SE   | `aarch64`    | Volumio OS | `4.239`    |                  | `1.0.0`         | :white_check_mark: | 2026-11-05 | @pettarin | Raspberry Pi 5       |

Notes

- A :white_check_mark: under the `Working?` column means that
  the service copiles, installs, and is able to control
  the mute and volume state of the RME ADI-2 device.
- The values in the `Architecture` column are output by
  `arch`, `uname -m` or `dpkg --print-architecture` commands.
  Note the synonyms: `aarch64 / arm64`, `x86_64 / amd64`.
- The values in the `ALSA Version` column are output by
  `cat /proc/asound/version` (and often coincide with
  the version of the running kernel).
- The values in the `Service Version` column are output by
  `alsa-rme-adi2-volume-control --version`.
- Volumio user space runs on 32-bit by design, even on 64-bit CPUs
  (while the kernel runs at 64-bit on 64-bit CPUs);
  therefore building the service on either a Raspberry Pi 3B+ (`armv7l`)
  or a Raspberry Pi 5 (`aarch64`) yields an `armhf` ELF executable,
  and the same executable built on the Pi 3B+ should work on the Pi 5.
