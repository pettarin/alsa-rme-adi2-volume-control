# How It Works

TODO: review this document

The `alsa-rme-adi2-volume-control` registers two ALSA controls
(`mixer_type=hardware`) for outputs:

- `Line Output 1/2`
- `Phones Output 3/4`

on the ALSA card corresponding to the RME ADI-2 device.

Each control has two elements:

- a boolean switch, for the mute/unmute state of the output; and
- an integer logical volume, from `0` to `100`, for the volume of the output.

Several service command line options
allow the user to specify the levels, in dB,
on the actual RME ADI-2 device,
corresponding to the `0` and `100` logical volume values,
along with the initial mute and volume state,
independently for each output:
- `--line12-min-db`,
- `--line12-max-db`,
- `--line12-switch`,
- `--line12-volume`,
- `--phones34-min-db`,
- `--phones34-max-db`,
- `--phones34-switch`,
- `--phones34-volume`.

The service then listens for events on the registered ALSA controls,
and whenever a mute/unmute or volume change is detected,
the corresponding SET command is sent to the RME ADI-2 device
through its MIDI interface.

The service terminates when killed (with `CTRL+C` or `kill/pkill`),
removing the ALSA controls it registered when it started.

> [!NOTE]
> In the current version of the service
> the synchronization of the state is one-way only:
> from ALSA to the physical RME ADI-2 device.
>
> If the user changes the mute or volume state on the RME ADI-2 device
> (e.g., using the volume knob or the IR remote),
> the ALSA controls will not be updated accordingly.
>
> This is a known limitation which will be addressed in a future release.

