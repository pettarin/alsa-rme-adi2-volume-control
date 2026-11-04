# Usage

Run with `--help` to print the full usage message:

```bash
alsa-rme-adi2-volume-control --help
Usage: alsa-rme-adi2-volume-control [options]

Creates ALSA mute and volume controls for RME ADI-2 devices.

Options:
  -C, --card-number INTEGER       Use the ALSA card with the given number; if specified, overrides the card detected by --device-name; option --list-cards prints the detected ones
  -d, --device-name STRING        RME device name (case sensitive, default: 'autodetect'); option --list-device-names prints the supported ones
  -D, --device-id STRING          RME device ID; if specified, overrides the device ID detected by --device-name; option --list-device-ids prints the supported ones
  -h, --help                      Print this help
      --license                   Print the license
  -k, --line12-switch INTEGER     Initial state for line output 1/2 (0=muted, 1=unmuted, default: 1)
  -K, --line12-volume INTEGER     Initial volume for line output 1/2 (in [0, 100], default: 50)
  -l, --line12-min-db DB          Minimum dB value for line output 1/2, corresponding to 0% volume (in [-93.8, 6.0], default: -60.0 dB)
  -L, --line12-max-db DB          Maximum dB value for line output 1/2, corresponding to 100% volume (in [-93.8, 6.0], default: -10.0 dB)
      --list-cards                List all ALSA cards and exit
      --list-device-ids           List all supported device IDs
      --list-device-names         List all supported device names
      --list-midi-devices         List all supported MIDI devices
  -M, --midi-device STRING        Use the specified MIDI device: 'autodetect' (default) or 'hw:X[,Y[,Z]]'; option --list-midi-devices prints the detected ones
      --no-line12                 Do not install controls for line output 1/2
      --no-phones34               Do not install controls for phones output 3/4
      --no-uninstall-on-exit      If specified, do not uninstall ALSA user-defined control elements when the program exits
  -o, --phones34-switch INTEGER   Initial state for phones output 3/4 (0=muted, 1=unmuted, default: 1)
  -O, --phones34-volume INTEGER   Initial volume for phones output 3/4 (in [0, 100], default: 50)
  -p, --phones34-min-db DB        Minimum dB value for phones output 3/4, corresponding to 0% volume (in [-93.8, 6.0], default: -60.0 dB)
  -P, --phones34-max-db DB        Maximum dB value for phones output 3/4, corresponding to 100% volume (in [-93.8, 6.0], default: -10.0 dB)
      --uninstall                 Uninstall ALSA user-defined control elements and exit
  -v, --verbose                   Enable verbose output
  -V, --version                   Print the name and version of this program and exit

Examples:
  alsa-rme-adi2-volume-control --list-cards
  alsa-rme-adi2-volume-control --list-midi-devices
  alsa-rme-adi2-volume-control --uninstall
  alsa-rme-adi2-volume-control --verbose --device-name "ADI-2 DAC"
  alsa-rme-adi2-volume-control --card-number 1 --device-id 0x73 --line12-min-db -25.0 --line12-max-db 0.0 --line12-volume 70 --line12-switch 1 --no-phones34
```

## How To ...

> [!TIP]
> The examples below were obtained on a PC running GNU/Linux Debian testing,
> with the RME ADI-2/4 Pro SE connected to it via USB.
>
> The actual output of the commands might depend on the host machine
> and/or operating system, and on the available audio devices attached to it.

### Print The Version

```bash
alsa-rme-adi2-volume-control --version
alsa-rme-adi2-volume-control 1.0.0 (build 2026-11-04)
```

### List All Supported Devices

```bash
alsa-rme-adi2-volume-control --list-device-id
ID      RME Product Family
0x71    ADI-2 DAC
0x72    ADI-2 Pro
0x73    ADI-2/4 Pro
```

```bash
alsa-rme-adi2-volume-control --list-device-names
RME Product Family (Device Name)
- 'autodetect'
- 'ADI-2 DAC'
- 'ADI-2 Pro'
- 'ADI-2/4 Pro'
```

### List All ALSA Cards

```bash
alsa-rme-adi2-volume-control --list-cards
Index   ID      Name
0       hw:0    HDA Intel PCH
1       hw:1    ADI-2/4 Pro SE (52102163)
```

Similar information can be obtained with `aplay -l`:

```bash
aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: PCH [HDA Intel PCH], device 0: ALC293 Analog [ALC293 Analog]
  Subdevices: 0/1
  Subdevice #0: subdevice #0
card 0: PCH [HDA Intel PCH], device 3: HDMI 0 [LG Ultra HD]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: PCH [HDA Intel PCH], device 7: HDMI 1 [HDMI 1]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: PCH [HDA Intel PCH], device 8: HDMI 2 [HDMI 2]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: SE52102163 [ADI-2/4 Pro SE (52102163)], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

### List All MIDI Devices

```bash
alsa-rme-adi2-volume-control --list-midi-devices
MIDI Device     Card Name
hw:1,0,0        ADI-2/4 Pro SE (52102163)
hw:1,0          ADI-2/4 Pro SE (52102163)
hw:1            ADI-2/4 Pro SE (52102163)
```

> [!NOTE]
> In the above example, `hw:1,0,0`, `hw:1,0`, and `hw:1` are all valid
> MIDI device identifiers associated to the same RME ADI-2/4 Pro SE device.

Similar information can be obtained with `amidi -l`:

```bash
amidi -l
Dir Device    Name
IO  hw:1,0,0  ADI-2/4 Pro SE (52102163) Port
```

### Enable Verbose Logging

```bash
alsa-rme-adi2-volume-control --verbose
[DEBU] Find ALSA card number and device ID from device name 'autodetect'
[DEBU] Find ALSA card by name matching query 'ADI-2/4 Pro'
[DEBU] Discovered ALSA card number '0' with name 'HDA Intel PCH'
[DEBU] Discovered ALSA card number '1' with name 'ADI-2/4 Pro SE (52102163)'
[DEBU] ALSA card number '1' with name 'ADI-2/4 Pro SE (52102163)' matches the query 'ADI-2/4 Pro': returning it
[DEBU] Found ALSA card number '1' for device ID '0x73' (ADI-2/4 Pro)
[DEBU] ALSA card number '1' explicitly specified by the user or already determined
[INFO] Using device ID '0x73'
[INFO] Using ALSA card '1'
[DEBU] Opened ALSA card 'hw:1'
...
```

### Specify The RME ADI-2 Device Explicitly

When run without arguments,
the service attempts to autodetect the RME ADI-2 device
(default: `--device-name autodetect`)
by probing all ALSA cards and matching the card name
against the supported device names
(as printed by `--list-device-ids` or `--list-device-names`).

The service uses the first (i.e., the lowest number) ALSA card
that matches one of the supported RME ADI-2 devices,
along with the corresponding MIDI device and RME device ID.
This approach should work for most users,
in particular those with only one RME ADI-2 device attached.

However, if the autodetection fails (please file a defect!)
or multiple RME ADI-2 devices are connected to the host machine,
the following options can be used
to bypass the autodetection of the RME ADI-2 device and
to specify an ALSA card/MIDI device/RME device ID explicitly:

- `--card-number`,
- `--device-id`,
- `--device-name` (with a value different than `autodetect`),
- `--midi-device`.

Examples:

```bash
# use the first ADI-2 Pro found, autodetecting the card number and MIDI device
alsa-rme-adi2-volume-control --device-name "ADI-2 Pro"

# use the ADI-2/4 Pro connected as ALSA card #1, autodetecting the MIDI device
alsa-rme-adi2-volume-control --card-number 1 --device-name "ADI-2/4 Pro"

# use the ADI-2 DAC (0x71) connected as ALSA card #5, using MIDI device hw:5,0,1
alsa-rme-adi2-volume-control --card-number 5 --device-id 0x71 --midi-device hw:5,0,1
```

### Control Only One Output

By default, controls for both outputs
(`Line Output 1/2` and `Phones Output 3/4`)
will be created.

To prevent controlling either output:

```bash
# control only Phones Output 3/4
alsa-rme-adi2-volume-control --no-line12

# control only Line Output 1/2
alsa-rme-adi2-volume-control --no-phones34
```

The service will exit immediately with an error
if both `--no-line12` and `--no-phones34` options
are specified.

### Limit The Min And Max Volume Levels

The minimum and maximum volume level, in dB, on the RME ADI-2 device
can be set independently for each of the two outputs,
with the following options:

| Short Option | Long Option         | Output            | Range (dB)     | Default Value (dB) | Notes                       |
|--------------|---------------------|-------------------|----------------|--------------------|-----------------------------|
| `-l`         | `--line12-min-db`   | Line Output 1/2   | `[-93.8, 6.0]` | `-60.0`            | Corresponds to volume `0`   |
| `-L`         | `--line12-max-db`   | Line Output 1/2   | `[-93.8, 6.0]` | `-10.0`            | Corresponds to volume `100` |
| `-p`         | `--phones34-min-db` | Phones Output 3/4 | `[-93.8, 6.0]` | `-60.0`            | Corresponds to volume `0`   |
| `-P`         | `--phones34-max-db` | Phones Output 3/4 | `[-93.8, 6.0]` | `-10.0`            | Corresponds to volume `100` |

Examples:

```bash
# set the min and max levels for Line Output 1/2 to -20.0 dB and 0.0 dB, respectively
alsa-rme-adi2-volume-control --line12-min-db -20.0 --line12-max-db 0.0

# set the min and max levels for Phones Output 3/4 to -50.0 dB and -30.0 dB, respectively
alsa-rme-adi2-volume-control --phones-min-db -50.0 --phones34-max-db -30.0

# both the above
alsa-rme-adi2-volume-control --line12-min-db -20.0 --line12-max-db 0.0 --phones-min-db -50.0 --phones34-max-db -30.0
```

The service will exit immediately with an error
if the provided values are invalid
(e.g., out of range, or min greater than max).

### Set The Initial State And Volume

The initial state (muted or unmuted) and volume
can be set independently for each of the two ALSA controls,
with the following options:

| Short Option | Long Option         | Output            | Allowed Values               | Default Value |
|--------------|---------------------|-------------------|------------------------------|---------------|
| `-k`         | `--line12-switch`   | Line Output 1/2   | `0` (muted) or `1` (unmuted) | `1`           |
| `-K`         | `--line12-volume`   | Line Output 1/2   | `[0, 100]`                   | `50.0`        |
| `-o`         | `--phones34-switch` | Phones Output 3/4 | `0` (muted) or `1` (unmuted) | `1`           |
| `-O`         | `--phones34-volume` | Phones Output 3/4 | `[0, 100]`                   | `50.0`        |

Examples:

```bash
# mute Line Output 1/2 with initial volume 30%
alsa-rme-adi2-volume-control --line12-switch 0 --line12-volume 30

# unmute Phones Output 3/4 with initial volume 70%
alsa-rme-adi2-volume-control --phones34-switch 1 --phones34-volume 70

# both the above
alsa-rme-adi2-volume-control --line12-switch 0 --line12-volume 30 --phones34-switch 1 --phones34-volume 70
```

The service will exit immediately with an error
if the provided values are invalid
(e.g., out of range or not `0` or `1`).

