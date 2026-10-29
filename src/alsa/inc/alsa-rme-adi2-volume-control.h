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

/* Boolean values */
#define BOOL_FALSE          0
#define BOOL_TRUE           1
#define BOOL_IS_TRUE(x)     ((x) == BOOL_TRUE)
#define BOOL_IS_FALSE(x)    (! BOOL_IS_TRUE(x))

/* Error codes: 0 is OK, anything else is an error */
#define ERR_OK                  0
#define ERR_ERROR_GENERIC       -1
#define ERR_CODE_IS_OK(x)       ((x) == ERR_OK)
#define ERR_CODE_IS_ERROR(x)    (! ERR_CODE_IS_OK(x))

/* ALSA error codes: 0 is OK, any negative value is an error */
#define ERR_ALSA_OK                 0
#define ERR_ALSA_CODE_IS_OK(x)      ((x) == ERR_ALSA_OK)
#define ERR_ALSA_CODE_IS_ERROR(x)   ((x) < ERR_ALSA_OK)

/* Exit codes for the whole service (0 OK, >1 error) */
#define EXIT_OK                             0
#define EXIT_ERROR_GENERIC                  1
#define EXIT_ERROR_USAGE_BAD_OPTION         2
#define EXIT_ERROR_USAGE_BAD_VALUE          3
#define EXIT_ERROR_CANNOT_FIND_CARD         4
#define EXIT_ERROR_CANNOT_OPEN_CARD         5
#define EXIT_ERROR_CANNOT_UNINSTALL         6
#define EXIT_ERROR_CANNOT_FIND_MIDI_PORT    7
#define EXIT_ERROR_CANNOT_OPEN_MIDI_PORT    8
#define EXIT_ERROR_CANNOT_ADD_CONTROL       9

/* The simple mixer name seen by amixer/MPD/Volumio is
   the common prefix "ADI2" of the two elements */
#define CONTROL_NAME        "ADI2 Playback Volume"
#define SWITCH_NAME         "ADI2 Playback Switch"

#define DEFAULT_CARD        "ADI-2"
#define DEFAULT_DEVICE_ID   RME_DEVICE_FAMILY_ADI2_DAC

/* Volume ranges for the logical ALSA mixer and the actual ADI-2 device */
#define VOL_MIN             0           /* Min volume for the logical ALSA mixer, mapped to VOL_MIN_DB on the actual ADI-2 device */
#define VOL_MAX             100         /* Max volume for the logical ALSA mixer, mapped to VOL_MAX_DB on the actual ADI-2 device */
#define VOL_DEFAULT         50          /* Default volume for the logical ALSA mixer: 50% */
#define VOL_MIN_DB          (-60.0)     /* Min level in dB on the actual ADI-2 device mapped from VOL_MIN of the logical ALSA mixer */
#define VOL_MAX_DB          (-10.0)     /* Max level in dB on the actual ADI-2 device mapped from VOL_MAX of the logical ALSA mixer */

#endif /* ALSA_RME_ADI2_VOLUME_CONTROL_H guard */

