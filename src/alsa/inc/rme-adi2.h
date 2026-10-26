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
#define RME_ADI2_H

/* From https://forum.rme-audio.de/viewtopic.php?pid=254806#p254806 :
 *
 * [After the introduction of the EX models in 2026]
 * The Device IDs are unchanged:
 *
 * 0x71    ADI-2 DAC, ADI-2 DAC EX
 * 0x72    ADI-2 Pro series, ADI-2 Pro EX
 * 0x73    ADI-2/4 Pro SE, ADI-2/4 Pro EX
 *
 * The Pro is identical in functionality.
 * The 2/4 Pro EX has digital input gain and RIAA-off option, plus input impedance choice.
 * To know if SE or EX is connected the 'device revision' is used (status message byte 6).
 * Updated MIDI tables will be published as soon as the 2/4 Pro EX is available.
 *
 */

/* RME device IDs */
#define RME_DEVICE_FAMILY_ADI2_DAC      0x71
#define RME_DEVICE_ADI2_DAC             0x71
#define RME_DEVICE_ADI2_DAC_EX          0x71
#define RME_DEVICE_ADI2_DAC_FS          0x71

#define RME_DEVICE_FAMILY_ADI2_PRO      0x72
#define RME_DEVICE_ADI2_PRO             0x72
#define RME_DEVICE_ADI2_PRO_EX          0x72
#define RME_DEVICE_ADI2_PRO_FS          0x72
#define RME_DEVICE_ADI2_PRO_FS_BE       0x72

#define RME_DEVICE_FAMILY_ADI24_PRO     0x73
#define RME_DEVICE_ADI24_PRO_EX         0x73
#define RME_DEVICE_ADI24_PRO_SE         0x73

/* RME output parameters */
#define RME_PARAM_LINE              0x1B
#define RME_PARAM_PHONES            0x4B

/* RME mute off/on values */
#define RME_PARAM_MUTE              0x60
#define RME_VALUE_MUTE_OFF          0x00
#define RME_VALUE_MUTE_ON           0x01

#endif /* RME_ADI2_H guard */
