/*
 * alsa-rme-adi2-volume-control - ALSA plugin controlling the volume of a RME ADI-2 device via MIDI
 *   Copyright (C) 2026  Alberto Pettarin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * TODO write description here
 *
 */

#ifndef RME_ADI2_H
#include "rme-adi2.h"
#endif

#ifndef ALSA_RME_ADI2_VOLUME_CONTROL_H
#define ALSA_RME_ADI2_VOLUME_CONTROL_H

/* The simple mixer name seen by amixer/MPD/Volumio is
   the common prefix "ADI2" of the two elements */
#define CONTROL_NAME        "ADI2 Playback Volume"
#define SWITCH_NAME         "ADI2 Playback Switch"

#define DEFAULT_CARD        "ADI-2"
#define DEFAULT_DEVICE_ID   RME_DEVICE_ADI2_DAC

/* Volume ranges for the logical ALSA mixer and the actual ADI-2 device */
#define VOL_MIN             0           /* Min volume for the logical ALSA mixer, mapped to VOL_MIN_DB on the actual ADI-2 device */
#define VOL_MAX             600         /* Max volume for the logical ALSA mixer, mapped to VOL_MAX_DB on the actual ADI-2 device */
#define VOL_DEFAULT         300         /* Default volume for the logical ALSA mixer: 50% */
#define VOL_MIN_DB          (-70.0)     /* Min level in dB on the actual ADI-2 device mapped from VOL_MIN of the logical ALSA mixer */
#define VOL_MAX_DB          (-15.0)     /* Max level in dB on the actual ADI-2 device mapped from VOL_MAX of the logical ALSA mixer */

#endif /* ALSA_RME_ADI2_VOLUME_CONTROL_H guard */

