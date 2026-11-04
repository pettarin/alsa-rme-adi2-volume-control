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
  -h, --help                      Show this help
  -k, --line12-switch INTEGER     Initial state for line output 1/2   (0=muted, 1=unmuted, default: 1)
  -K, --line12-volume INTEGER     Initial volume for line output 1/2   (in [0, 100], default: 50)
  -l, --line12-min-db DB          Minimum dB value for line output 1/2, corresponding to   0% volume (default: -60.0 dB)
  -L, --line12-max-db DB          Maximum dB value for line output 1/2, corresponding to 100% volume (default: -10.0 dB)
      --list-cards                List all ALSA cards and exit
      --list-device-ids           List all supported device IDs
      --list-device-names         List all supported device names
      --list-midi-devices         List all supported MIDI devices
  -M, --midi-device STRING        Use the specified MIDI device: 'autodetect' or 'hw:X[,Y[,Z]]'; option --list-midi-devices prints the detected ones
      --no-line12                 Do not install controls for line output 1/2
      --no-phones34               Do not install controls for phones output 3/4
      --no-uninstall-on-exit      If specified, do not uninstall ALSA user-defined control elements when the program exits
  -o, --phones34-switch INTEGER   Initial state for phones output 3/4 (0=muted, 1=unmuted, default: 1)
  -O, --phones34-volume INTEGER   Initial volume for phones output 3/4 (in [0, 100], default: 50)
  -p, --phones34-min-db DB        Minimum dB value for phones output 3/4, corresponding to   0% volume (default: -60.0 dB)
  -P, --phones34-max-db DB        Maximum dB value for phones output 3/4, corresponding to 100% volume (default: -10.0 dB)
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





By default, controls for both outputs
(`Line Output 1/2` and `Phones Output 3/4`)
will be created.
Use `--no-line12` or `--no-phones34` to prevent controlling either output.

When run without argument, the RME ADI-2 device
will be autodetected (`--device-name autodetect`),
using the first (i.e., the lowest index) ALSA card
that matches one of the supported RME ADI-2 devices,
along with the corresponding MIDI device.
This should work for most users,
in particular those with only one RME ADI-2 device attached.

Use the `--card-number`, `--device-id` or `--device-name`, and `--midi-device`
options to force a specific ALSA card, RME device ID, and MIDI device,
for instance if there are more than one RME ADI-2 devices attached.
For example:

```bash
# use the first ADI-2 Pro found, autodetecting the MIDI device
alsa-rme-adi2-volume-control --device-name "ADI-2 Pro"

# use the ADI-2/4 Pro connected as ALSA card #1, autodetecting the MIDI device
alsa-rme-adi2-volume-control --card-index 1 --device-name "ADI-2/4 Pro"

# use the ADI-2 DAC connected as ALSA card #5, using MIDI device hw:5,0,1
alsa-rme-adi2-volume-control --card-index 5 --device-id 0x71 --midi-device hw:5,0,1
```

To enable verbose logging, pass the `--verbose` option.

#### Listing ALSA Cards And MIDI Devices

To list all the ALSA cards available on the system,
attach your RME ADI-2 device and power it on,
then run the command with the `--list-cards` option:

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

Similarly, to list all the MIDI devices,
attach your RME ADI-2 device and power it on,
then run the command with the `--list-midi-devices` option:

```bash
alsa-rme-adi2-volume-control --list-midi-devices
MIDI Device     Card Name
hw:1,0,0        ADI-2/4 Pro SE (52102163)
hw:1,0          ADI-2/4 Pro SE (52102163)
hw:1            ADI-2/4 Pro SE (52102163)
```

Similar information can be obtained with `amidi -l`:

```bash
amidi -l
Dir Device    Name
IO  hw:1,0,0  ADI-2/4 Pro SE (52102163) Port
```

> [!NOTE]
> In the above examples, `hw:1,0,0`, `hw:1,0`, and `hw:1` are all valid
> MIDI device identifiers associated to the same RME ADI-2/4 Pro SE device.

> [!TIP]
> The above examples were obtained on a PC running GNU/Linux Debian testing,
> with the RME ADI-2/4 Pro SE connected to it via USB.


