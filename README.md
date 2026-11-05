# alsa-rme-adi2-volume-control

A background service providing ALSA controls
to change the mute and volume state
of an RME ADI-2 device via its MIDI interface.


## Overview

This repository contains the source code of `alsa-rme-adi2-volume-control`,
a C program designed to be executed as a background service
on a modern Linux machine with
[ALSA](https://www.alsa-project.org/),
exposing user-defined control elements to change the mute and volume state
of an [RME](https://rme-audio.de/home.html) ADI-2 device
connected to the running machine via USB,
via its MIDI interface.

The main purpose of `alsa-rme-adi2-volume-control`
consists in exposing such ALSA user-defined control elements
to client applications (e.g.,
[MPD](https://www.musicpd.org/),
[MOC](https://moc.daper.net/),
[moOde](https://moodeaudio.org/),
[Volumio](https://volumio.com/),
etc.) allowing the end user to change the mute and volume state
directly from the client application while
leveraging the excellent hardware-based volume controls
of the RME ADI-2 device itself,
instead of using software-based transformations
which might impact the quality of the reproduced audio stream.

> [!NOTE]
> In the current version of the service
> the synchronization of the state is one-way only:
> from ALSA to the physical RME ADI-2 device.
> If the user changes the mute or volume state on the RME ADI-2 device
> (e.g., using the volume knob or the IR remote),
> the ALSA controls will not be updated accordingly.
>
> This is a known limitation which will be addressed in a future release.

Some additional support files are provided in this repository:

- a `systemd` service manifest to run the service in background
  on modern Debian-based Linux distributions;
- a `udev` rules definition file that allows applications
  in user-space to interact with an RME ADI-2 device
  without requiring elevated privileges;
- (TODO) a Bash script to automate installing and uninstalling
  the required files on a modern Debian-based Linux distribution.

> [!WARNING]
> The Volumio plugin is not available yet.
> TODO: when it is, change this WARNING into a NOTE.
>
> The source code of the Volumio plugin
> that installs the `alsa-rme-adi2-volume-control` service
> on a running Volumio device can be found in the
> [Volumio (Bookworm) Plugins](https://github.com/volumio/volumio-plugins-sources-bookworm)
> repository.
> Consult the documentation there for the details
> on how to install and operate the plugin.

### Screenshots

Service running in foreground,
with `alsamixer` showing the registered ALSA controls:

![Service running in foreground](res/screenshot_service_running.png)

Service after its termination,
with `alsamixer` no longer showing the registered ALSA controls:

![Service after its termination](res/screenshot_service_exited.png)

### Supported RME ADI-2 Devices

The following families of RME ADI-2 devices are supported:

- [ADI-2 DAC](https://rme-audio.de/adi-2-dac.html) (device ID: `0x71`)
- [ADI-2 Pro](https://rme-audio.de/converter/adi-2-pro-ex.html) (device ID: `0x72`)
- [ADI-2/4 Pro](https://rme-audio.de/converter/adi-2-4-pro-ex.html) (device ID: `0x73`)

All device "variants" of each family should be supported
(e.g., `ADI-2 Pro AE`, `ADI-2 Pro FS`, `ADI-2 Pro FS BE`, and `ADI-2 Pro EX`
are all `ADI-2 Pro`),
since they share the same RME device ID
and the same MIDI interface for setting mute/volume parameters.

> [!WARNING]
> The author tested the service with an RME ADI-2/4 Pro SE device,
> on both Debian testing on a PC (`amd64`) and
> Volumio 4.119 on a Raspberry Pi 5 (`armhf`).
>
> Other RME ADI-2 devices should work,
> but mind that they have not been tested directly by the author.


## Installation

> [!TIP]
> Expert users and developers might want to consult the
> [DEVELOPMENT](docs/DEVELOPMENT.md)
> documentation if they prefer recompiling the program from sources
> and/or manually installing the service.

> [!WARNING]
> Installing the service as described below requires `sudo` privileges.

To install the service from the latest released package:

```bash
# inspect the downloaded install.sh script before running it with sudo!
curl -s -O "https://www.albertopettarin.it/alsa-rme-adi2-volume-control/repo/alsa-rme-adi2-volume-control/install.sh"
sudo bash install.sh
```

To install the service from a local clone of this Git repository,
compiling the service executable locally:

```bash
git clone https://github.com/pettarin/alsa-rme-adi2-volume-control.git
cd alsa-rme-adi2-volume-control
sudo bash install.sh --from-source
```

The above commands will perform the following actions:

- uninstall a previous installation of the service, if present;
- download (default) or compile (`--from-source`) the service executable;
- install the service executable into `/usr/local/bin/`;
- install the `systemd` service and start it;
- install the `udev` rules allowing hot plugging RME ADI-2 devices.

To uninstall a previous installation of the service
(without trying to install it again):

```bash
sudo bash install.sh --uninstall
```

The following commands are useful to check
whether the installation of `alsa-rme-adi2-volume-control`
was successful:

```bash
# select the appropriate ALSA card: it is possible to mute/unmute
# and to change the volume of the RME ADI-2 device.
alsamixer

# check the status of the systemd service
# (it should say: "enabled" and "active (running)"
systemctl status alsa-rme-adi2-volume-control

# watch the systemd service log
journalctl -n 100 -f -u alsa-rme-adi2-volume-control
```

The complete usage documentation of the service executable
can be found in the
[USAGE](docs/USAGE.md)
file.


## License

This project is licensed under
the GNU General Public License v3.0 (GPLv3).

See the
[LICENSE](https://github.com/pettarin/alsa-rme-adi2-volume-control/blob/main/LICENSE)
file for details.


## Authors

- Alberto Pettarin ([Web](https://www.albertopettarin.it))

The approach of this project was inspired by
[rme-adi2-pro-volume](https://github.com/JendaT/rme-adi2-pro-volume)
by
[Jenda Třešňák (JendaT)](https://github.com/JendaT),
although eventually the Python service was dropped,
and the code of the C service was rewritten from scratch.


## Legal Disclaimers

RME is a registered trademark of RME Intelligent Audio Solutions,
a company registered in Germany (VAT ID: DE813455918).

Please refer to the
[RME Imprint](https://rme-audio.de/imprint.html).

This project and its authors are not affiliated
nor endorsed by RME.

