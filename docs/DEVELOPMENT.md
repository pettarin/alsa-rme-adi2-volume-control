# Development

> [!TIP]
> This document is intended for expert users and developers only.
>
> Regular users should take advantage of the automated installation
> described in the
> [main README](../README.md)
> file of the project.


## Project Structure

```
.
├── docs                                            <- documentation
│   ├── CHANGELOG.md                                <- releases and their changes
│   ├── CODE_OF_CONDUCT.md                          <- ground rules of the project spaces
│   ├── CONTRIBUTING.md                             <- how to report issues and contribute code
│   ├── DEVELOPMENT.md                              <- this file
│   ├── ROADMAP.md                                  <- planned work, broken down by future milestone
│   └── SECURITY.md                                 <- supported versions and how to report a vulnerability
├── imgs                                            <- images for the main README file
...
├── install.sh                                      <- install script
├── LICENSE                                         <- full text of the license for this project
├── README.md                                       <- main README file
├── res                                             <- additional resources for this project
...
├─── src                                            <- source files
│    ├── alsa                                       <- source files for the executable
│    │   ├── bin                                    <- the compiled executable will be written here
│    │   │   └── .keep
│    │   ├── CMakeLists.txt                         <- CMake project
│    │   ├── inc
│    │   │   ├── alsa-rme-adi2-volume-control.h     <- main C header file
│    │   │   └── rme-adi2.h                         <- C header containing RME ADI-2 specific declarations
│    │   └── src
│    │       └── alsa-rme-adi2-volume-control.c     <- main C source file
│    ├── systemd
│    │   └── alsa-rme-adi2-volume-control.service   <- systemd unit file
│    └── udev
│        └── 99-alsa-rme-adi2-volume-control.rules  <- udev rules definition file
└── VERSION                                         <- current version of this project
```


## Branching And Versioning Policy

### Branch `main`

- Branch `main` is protected, and it represents the sources of the latest stable release.
- Only the maintainer is allowed to push there directly,
  usually merging the `devel` branch when preparing a new release.
- Published releases are from commit tagged `vX.Y.Z`.

### Branch `devel`

- Branch `devel` is protected, and it accumulates fixes and features for the next release.
- The maintainer is allowed to push there directly,
  usually merging feature or fix branches.
- Other contributors should open pull requests
  (see the [CONTRIBUTING](CONTRIBUTING.md) document)
  against the `devel` branch, which will be reviewed and, if appropriate, merged.

### Other Branches

- Fix branches should be named `fix/gh_#123_short_description`
  where `#123` is the ID of the GitHub issue being fixed.
- Feature branches should be named `feature/gh_#456_short_description`
  where `#456` is the ID of the GitHub issue describing the requested feature.
- In both cases, it is mandatory to have a GitHub issue
  describing the issue being fixed or the new feature being added.
- No need to squash commits on a fix or feature branch,
  just try to have meaningful commit messages if you have more than one commit.
  Usually it is preferable referencing the GitHub issue
  (`Fixes #123 ...` or `Implement #456 ...`) in the commit message.

### Versioning

- This project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## Contributing

See the
[CONTRIBUTING](CONTRIBUTING.md)
document.


## Manual Installation

### Compile, Test, And Install The Service Program

#### Requirements

- [ALSA](https://www.alsa-project.org/wiki/Main_Page)
- [CMake](https://cmake.org/)
- [GNU C Compiler (GCC)](https://www.gnu.org/software/gcc/)
- [GNU Make](https://www.gnu.org/software/make/)
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)

On `deb`-based Linux distributions (e.g., Debian, RaspiOS, Ubuntu)
the above requirements can be installed with the following command:

```bash
sudo apt install gcc cmake make libasound2-dev pkg-config
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
run it in foreground:

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
select the card (hit `F6`) corresponding your RME ADI-2 device
and start making changes to the user-defined control elements.

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

To terminate the service, simply hit `CTRL+C` on the console running it
or use the `pkill`/`kill` command with the `SIGTERM` signal:

```bash
./bin/alsa-rme-adi2-volume-control
...

# hit CTRL+C
^C
[INFO] Control loop exited with return code '0'
```

For details on all the available command line options, see the
[USAGE](USAGE.md)
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
# copy the new systemd service manifest to /lib/systemd/system/
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
# copy the new udev rules definition file to /etc/udev/rules.d/
sudo cp src/udev/99-alsa-rme-adi2-volume-control.rules /etc/udev/rules.d/

# reload and trigger rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```
