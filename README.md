# alsa-rme-adi2-volume-control

ALSA plugin providing virtual mixers that can control
the volume of devices of the RME ADI-2 family
via their MIDI interface.


## Overview

TODO write this


## Automated Installation

TODO write this


## Manual Installation

> [!TIP]
> This section is intended for expert users and developers only.
>
> Regular users should take advantage of the automated installation
> described in the previous section.

### Compile And Install The ALSA Plugin

#### Requirements

- ALSA
- CMake

#### Compile

```bash
cd src/alsa
cmake .
make
```

The executable is written to `src/alsa/bin/alsa-rme-adi2-volume-control`.

To test it, run a command similar to the following:

```bash
bin/alsa-rme-adi-2-volume-control --verbose --card ADI-2 --device 0x73 --output line --min-db -60.0 --max-db 0.0
Found card 1: ADI-2/4 Pro SE (52102163)
Using card 1
Using MIDI port hw:1,0,0
Created control 'ADI2 Playback Volume' numid=9
Created control 'ADI2 Playback Switch' numid=10
RME ADI-2 control bridge started
Device ID: 0x73, Output: line
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
-30.0 dB -> SysEx: F0 00 20 0D 73 02 1B 1D 54 F7
Volume: -30.0 dB
Monitoring controls 'ADI2 Playback Volume' and 'ADI2 Playback Switch' for changes...
```

and, on a different shell, run `alsamixer` and
change the volume of the `ADI2` control.

You should observe log messages similar to the following ones,
and the volume of the selected output (line 1/2 in the example)
change on your RME ADI-2 device:

```bash
bin/alsa-rme-adi-2-volume-control --verbose --card ADI-2 --device 0x73 --output line --min-db -60.0 --max-db 0.0
...

# changing the ADI2 control to 80%
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
-12.0 dB -> SysEx: F0 00 20 0D 73 02 1B 1F 08 F7
Volume: -12.0 dB

# changing the ADI2 control to 85%, 90%, 95% and 100%
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
-8.9 dB -> SysEx: F0 00 20 0D 73 02 1B 1F 28 F7
Volume: -8.9 dB
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
-5.9 dB -> SysEx: F0 00 20 0D 73 02 1B 1F 46 F7
Volume: -5.9 dB
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
-2.9 dB -> SysEx: F0 00 20 0D 73 02 1B 1F 64 F7
Volume: -2.9 dB
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
0.0 dB -> SysEx: F0 00 20 0D 73 02 1B 00 00 F7
Volume: 0.0 dB

# mute
Mute on -> SysEx: F0 00 20 0D 73 02 1B 60 01 F7

# unmute
Mute off -> SysEx: F0 00 20 0D 73 02 1B 60 00 F7
0.0 dB -> SysEx: F0 00 20 0D 73 02 1B 00 00 F7
Volume: 0.0 dB
```

#### Install

> [!NOTE]
> Installing the executable, that is, copying it to `/usr/local/bin/`,
> requires `sudo` privileges.

```bash
sudo make install
```

### Install The `systemd` Service

> [!NOTE]
> Installing the `systemd` service requires `sudo` privileges.

```bash
# copy the new systemd file to /etc/systemd/system/
sudo cp src/systemd/alsa-rme-adi2-volume-control.service /etc/systemd/system

# reload the systemd daemon
sudo systemctl daemon-reload

# load the service
sudo systemctl restart alsa-rme-adi2-volume-control
```

### Install The `udev` Rules

> [!NOTE]
> Installing the `udev` rules requires `sudo` privileges.

```bash
# copy the new rules file to /etc/udev/rules.d/
sudo cp src/udev/99-alsa-rme-adi2-volume-control.rules /etc/udev/rules.d/

# reload and trigger rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```


## License

This project is licensed under
the GNU General Public License v3.0 (GPLv3).

See the
[LICENSE](https://github.com/pettarin/alsa-rme-adi2-volume-control/blob/main/LICENSE)
file for details.


## Authors

- Alberto Pettarin ([Web](https://www.albertopettarin.it))

This project has been heavily inspired by
[rme-adi2-pro-volume](https://github.com/JendaT/rme-adi2-pro-volume)
by
[Jenda Třešňák (JendaT)](https://github.com/JendaT).


## Legal Disclaimers

RME is a registered trademark of RME Intelligent Audio Solutions,
a company registered in Germany (VAT ID: DE813455918).

Please refer to the
[RME Imprint](https://rme-audio.de/imprint.html).

This project and its authors are not affiliated
nor endorsed by RME.

