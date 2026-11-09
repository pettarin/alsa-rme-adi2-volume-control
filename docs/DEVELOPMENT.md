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
├── src                                            <- source files
│   ├── alsa                                       <- source files for the executable
│   │   ├── bin                                    <- the compiled executable will be written here
│   │   │   └── .keep
│   │   ├── CMakeLists.txt                         <- CMake project
│   │   ├── inc
│   │   │   ├── alsa-rme-adi2-volume-control.h     <- main C header file
│   │   │   └── rme-adi2.h                         <- C header containing RME ADI-2 specific declarations
│   │   └── src
│   │       └── alsa-rme-adi2-volume-control.c     <- main C source file
│   ├── systemd
│   │   ├── 99-alsa-rme-adi2-volume-control.preset <- systemd preset file
│   │   └── alsa-rme-adi2-volume-control.service   <- systemd service unit file
│   ├── systemd
│   └── udev
│       └── 99-alsa-rme-adi2-volume-control.rules  <- udev rules definition file
└── VERSION                                         <- current version of this project
```

The main contents of this repository are:

- in the `src/alsa/` directory: the C source files
  for the executable `alsa-rme-adi2-volume-control`,
  along with the CMake project to compile them;
- in the `src/systemd/` directory: the `systemd` service unit file
  to run the service in background
  on modern Debian-based Linux distributions;
- in the `src/udev/` directory:  a `udev` rules definition file
  that restarts the `systemd` service whenever
  an RME ADI-2 device is connected,
  thus handling the case where a device is turned off and on again,
  or physically disconnected and connected again;
- `install.sh` a Bash script to automate installing and uninstalling
  the required files on a modern Debian-based Linux distribution.


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

## How The `alsa-rme-adi2-volume-control` Executable Works

This section explains how the `alsa-rme-adi2-volume-control` executable works.

First, the command line arguments are parsed,
and if any unknown option or invalid values are provided,
the execution terminates with an error (non-zero) exit code.

If one of the following options is provided,
the corresponding action is performed,
and the execution terminates with the success (zero) exit code:

- `--help`,
- `--license`,
- `--list-cards`,
- `--list-device-ids`,
- `--list-device-names`,
- `--list-midi-devices`, or
- `--version`.

Then, the program determines
the actual RME device ID and ALSA card number to be used:
if the user provided them with options
`--device-name / --device-id` and `--card-number`,
the provided values will be used.
If the user provided only `--device-name` but not `--card-number`,
or `--device-name` is `autodetect` (default),
then the program will use the device
corresponding to the ALSA card with lowest number
whose name matches the provided `--device-name`
(and, if `--device-name` is `autodetect` (default),
then the know RME ADI-2 device names will be attempted in this order:
first `ADI-2/4 Pro`, then `ADI-2 Pro`, and finally `ADI-2 DAC`).
If no ALSA card can be found matching the provided RME device
or ALSA card number (e.g., no RME ADI-2 device connected,
or wrong command line arguments have been provided),
the execution terminates with an error.

Then, any ALSA user-defined control elements from a prior execution
are removed from the determined ALSA card.

If the `--uninstall` option is provided, the execution terminates
with the success (zero) exit code.
Otherwise, the program tries to open the MIDI device associated
with the selected ALSA card: either the one explicitly provided by the user
with the `--midi-device` option, or,
if `--midi-device` is `autodetect` (default),
it will open the first (lowest-index) available MIDI device with name
`hw:N,x,y`, `hw:N,x`, or `hw:N`
where `N` is the ALSA card number.

If no such MIDI device can be opened
(e.g., because the device is already controlled by another process,
or wrong command line arguments have been provided),
the execution terminates with an error.

> [!NOTE]
> Running the `alsa-rme-adi2-volume-control` program
> without arguments implies the autodetection of all three:
> RME device ID, ALSA card number, and MIDI device,
> which should work for most users, chiefly those
> with only one RME ADI-2 device connected.
>
> For a more detailed discussion and usage examples of the options
> to override this autodetection mechanism, please refer to the
> [Usage](USAGE.md) document.

If the ALSA card and MIDI device have been successfully opened,
the program registers ALSA user-defined control elements,
two for each of the controlled outputs:

- a boolean switch, for muting/unmuting the output;
- an integer value with range `[0, 100]` representing the volume
  of the output.

The controlled outputs are two:

- `Line Output 1/2`, and
- `Phones Output 3/4`,

unless one of `--no-line12` or `--no-phones34` options were specified
(in that case, the mentioned output is not controlled, meaning
the corresponding ALSA user-defined control elements are not registered).

Specifically, the names (IDs) of the control elements are
(see the main C header `alsa-rme-adi2-volume-control.h`):

- `Line Output 1/2 Playback Switch`,
- `Line Output 1/2 Playback Volume`,
- `Phones Output 3/4 Playback Switch`, and
- `Phones Output 3/4 Playback Volume`,

resulting in the "unified" (mute/unmute + volume value)
controls `Line Output 1/2` and `Phones Output 3/4` in `alsamixer`.

Finally, a monitor loop is started.
The loop watches any ALSA event, and it consumes those events
related to the registered ALSA user-defined control elements.
One such event can be
 "Line Output 1/2 Playback Switch (new) value is 1"
or
"Phones Output 3/4 Playback Volume (new) value is 70".

The program logic translates each of the relevant ALSA events
into the appropriate MIDI SysEx call, which is sent
to the RME ADI-2 device, thus achieving hardware-based
control of mute and volume state.

The monitor loop terminates whenever the RME ADI-2 device
is disconnected or the program receives a `SIGTERM` interrupt
(or it is killed).
Clean-up logic uninstalls the ALSA user-defined control elements,
unless the `--no-uninstall-on-exit` option was specified.

Note that the ALSA volume values are always between `0` and `100`.
They are translated to the corresponding dB level on the RME ADI-2 device
by linear interpolation between the values specified by options:

- `--line12-min-db` (default: `-60.0` dB) and
- `--line12-max-db` (default: `-10.0` dB) for `Line Output 1/2`, and
- `--phones34-min-db` (default: `-60.0` dB) and
- `--phones34-max-db` (default: `-10.0` dB) for `Phones Output 3/4`.

The initial mute/unmute state and volume value can be set using options:

- `--line12-switch` (default: `1 == unmuted`),
- `--line12-volume` (default: `50` == 50%),
- `--phones34-switch` (default: `1 == unmuted`),
- `--phones34-volume` (default: `50` == 50%).

Please refer to the
[Usage](USAGE.md)
document for a more detailed discussion and examples of these options.

> [!NOTE]
> In the current version of the service
> the synchronization of the state is one-way only:
> from ALSA to the physical RME ADI-2 device.
> If the user changes the mute or volume state on the RME ADI-2 device
> (e.g., using the volume knob or the IR remote),
> the ALSA controls will not be updated accordingly.
>
> This is a known limitation which will be addressed in a future release.

