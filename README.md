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

