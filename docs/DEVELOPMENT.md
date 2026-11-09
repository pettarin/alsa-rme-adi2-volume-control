# Development

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

