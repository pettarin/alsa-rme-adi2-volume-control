# Development

## Manual Installation

> [!TIP]
> This section is intended for expert users and developers only.
>
> Regular users should take advantage of the automated installation
> described in the
> [main README](../README.md)
> file of the project.

### Compile, Test, And Install The Service Program

#### Requirements

- [ALSA](https://www.alsa-project.org/wiki/Main_Page)
- [CMake](https://cmake.org/)
- [GNU C Compiler (GCC)](https://gcc.gnu.org/)

On `deb`-based Linux distributions (e.g., Debian, RaspiOS, Ubuntu)
the above requirements can be installed with the following command:

```bash
sudo apt install build-essential cmake libasound2-dev
```

> [!WARNING]
> Running the above command requires `sudo` privileges.

#### Compile

```bash
cd src/alsa
cmake .
make
```

The executable is written to `src/alsa/bin/alsa-rme-adi2-volume-control`.

#### Test

To test the `alsa-rme-adi2-volume-control` executable,
run it in foreground, with a command similar to the following:

```bash
./bin/alsa-rme-adi2-volume-control
[INFO] Using device ID '0x73'
[INFO] Using ALSA card '1'
[INFO] Using MIDI device 'hw:1,0,0'
[INFO] Start control loop for device ID '0x73' on MIDI device 'hw:1,0,0' of ALSA card number '1'
[INFO] SET mute off on line12
[INFO] SET volume 50 == -35.0 dB on line12
[INFO] SET mute off on phones34
[INFO] SET volume 50 == -35.0 dB on phones34
[INFO] Monitoring ALSA user-defined control elements 'Line Output 1/2 Volume' and 'Line Output 1/2 Switch' for changes
[INFO] Monitoring ALSA user-defined control elements 'Phones Output 3/4 Volume' and 'Phones Output 3/4 Switch' for changes
...
```

and, on a different shell, run `alsamixer`,
selecting the card (hit `F6`) corresponding your RME ADI-2 device.

You should observe in the service log messages similar to the following ones,
and the volume of the selected output change on your RME ADI-2 device.

```bash
./bin/alsa-rme-adi2-volume-control
...
# mute "Line Output 1/2" in alsamixer
[INFO] SET mute on on line12

# unmute "Line Output 1/2" in alsamixer
[INFO] SET mute off on line12
[INFO] SET volume 50 == -35.0 dB on line12

# set "Line Output 1/2" to 90% in alsamixer
[INFO] SET mute off on line12
[INFO] SET volume 90 == -15.0 dB on line12

# set "Phones Output 3/4" to 70% in alsamixer
[INFO] SET mute off on phones34
[INFO] SET volume 70 == -25.0 dB on phones34

# mute "Phones Output 3/4" in alsamixer
[INFO] SET mute on on phones34

# unmute "Phones Output 3/4" in alsamixer
[INFO] SET mute off on phones34
[INFO] SET volume 70 == -25.0 dB on phones34
```

To terminate the service, simply hit `CTRL+C`
or use the `kill` or `pkill` commands with the `SIGTERM` signal:

```bash
./bin/alsa-rme-adi2-volume-control
...

# hit CTRL+C
^C
[INFO] Control loop exited with return code '0'
```

For details on all the available command line options, see the
[Usage](./USAGE.md)
document.

#### Install

> [!WARNING]
> Installing the executable, that is, copying it to `/usr/local/bin/`,
> requires `sudo` privileges.

```bash
sudo make install
```

### Install The `systemd` Service

> [!WARNING]
> Installing the `systemd` service requires `sudo` privileges.

```bash
# copy the new systemd file to /lib/systemd/system/
sudo cp src/systemd/alsa-rme-adi2-volume-control.service /lib/systemd/system/

# reload the systemd daemon
sudo systemctl daemon-reload

# enable and (re)start the service
sudo systemctl enable alsa-rme-adi2-volume-control
sudo systemctl restart alsa-rme-adi2-volume-control
```

### Install The `udev` Rules

> [!WARNING]
> Installing the `udev` rules requires `sudo` privileges.

```bash
# copy the new rules file to /etc/udev/rules.d/
sudo cp src/udev/99-alsa-rme-adi2-volume-control.rules /etc/udev/rules.d/

# reload and trigger rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

