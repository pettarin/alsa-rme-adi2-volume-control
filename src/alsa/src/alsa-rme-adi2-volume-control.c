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
 */

#include <stdio.h>                          /* for stderr, stdout, etc. */
#include <stdlib.h>                         /* for atof, atoi, etc. */
#include <string.h>                         /* for str* */
#include <signal.h>                         /* for sig* */
#include <unistd.h>                         /* for optarg */
#include <getopt.h>                         /* for getopt_long() */
#include <poll.h>                           /* for poll() */
#include <errno.h>                          /* for errno, EINTR */
#include <alsa/asoundlib.h>                 /* for snd* */

#include "rme-adi2.h"                       /* RME ADI-2 specific constants */
#include "alsa-rme-adi2-volume-control.h"   /* service constants and macros */


/* *************************************************************************
 * Global state variables
 * ************************************************************************* */

/* enable verbose log */
static int gbLogVerbose = BOOL_FALSE;

/* flag tracking whether the service is running or not */
static volatile sig_atomic_t gbServiceIsRunning = (sig_atomic_t)BOOL_TRUE;

/* service configuration */
static struct {
    char card_name[64];
    int card_num;
    char midi_port[32];
    int device_id;
    int output_param;
    double min_db;
    double max_db;
    int bUninstallOnly;
    int bUninstallOnExit;
} goConfiguration = {
    .card_name = DEFAULT_CARD_NAME,
    .card_num = -1,
    .midi_port = "",
    .device_id = DEFAULT_DEVICE_ID,
    .output_param = RME_PARAM_LINE,
    .min_db = DEFAULT_VOLUME_MIN_DB,
    .max_db = DEFAULT_VOLUME_MAX_DB,
    .bUninstallOnly = DEFAULT_UNINSTALL_ONLY,
    .bUninstallOnExit = DEFAULT_UNINSTALL_ON_EXIT,
};


/* *************************************************************************
 * Log helpers
 * ************************************************************************* */

static void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[INFO] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}

static void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ERRO] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
}

static void log_verbose(const char *fmt, ...) {
    if (BOOL_IS_FALSE(gbLogVerbose)) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[DEBU] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}


/* *************************************************************************
 * ALSA card helpers
 * ************************************************************************* */

/* Find card number by name substring */
static int find_card_by_name(const char *name) {
    int card = -1;
    char *card_name;

    log_verbose("Find card by name with query '%s'", name);

    while (snd_card_next(&card) >= 0 && card >= 0) {
        if (snd_card_get_name(card, &card_name) < 0) {
            continue;
        }
        log_verbose("Found card '%d' with name '%s'", card, card_name);
        if (strstr(card_name, name) != NULL) {
            log_verbose(
                "Card '%d' with name '%s' matches the query '%s'",
                card,
                card_name,
                name
            );
            free(card_name);
            return card;
        }
        free(card_name);
    }
    return -1;
}


/* *************************************************************************
 * ALSA user-defined control elements helpers
 * ************************************************************************* */

/* Uninstall an ALSA user-defined control element (UDCE)
 * specified by name from the given card ctl. */
static int uninstall_alsa_udce_by_name(
    snd_ctl_t *ctl,
    const char *name
) {
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    int iRet = ERR_OK;

    iRet = snd_ctl_elem_remove(ctl, id);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_verbose("Unable to remove control with name '%s'", name);
        return ERR_ERROR_GENERIC;
    }

    log_verbose("Removed control with name '%s'", name);
    return ERR_OK;
}

/* Uninstall all ALSA user-defined control elements (UDCEs)
 * from the given card ctl.
 *
 * If bIgnoreErrors is true, errors will be ignored,
 * and ERR_OK will be always returned. */
static int uninstall_all_alsa_udce_s(
    snd_ctl_t *ctl,
    int bIgnoreErrors
) {
    static const char *sControlNames[] = {
        SWITCH_NAME,
        CONTROL_NAME
    };
    int iRemovedCount = 0;
    int iRet = ERR_OK;
    int iRetInner = ERR_OK;

    size_t iNumberOfControls = sizeof(sControlNames) / sizeof(sControlNames[0]);
    for (size_t i = 0; i < iNumberOfControls; i++) {
        iRetInner = uninstall_alsa_udce_by_name(ctl, sControlNames[i]);
        if (ERR_CODE_IS_OK(iRetInner)) {
            iRemovedCount += 1;
        } else {
            iRet = ERR_ERROR_GENERIC;
        }
    }
    log_verbose(
        "Removed %d out of %d user-defined control element(s)",
        iRemovedCount,
        iNumberOfControls
    );

    if (BOOL_IS_TRUE(bIgnoreErrors) && ERR_CODE_IS_ERROR(iRet)) {
        log_verbose(
            "The above error(s) occurred while trying to uninstall "
            "user-defined control elements can be ignored"
        );
        iRet = ERR_OK;
    }

    return iRet;
}

/* Install a new control (identified by id and name) on the card ctl.
 * The control is a boolean switch (mute) if bElementIsSwitch is true,
 * otherwise it is an integer value (volume). */
static int install_control_element(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *id,
    const char *name,
    int bElementIsSwitch
) {
    snd_ctl_elem_info_t *info = NULL;
    snd_ctl_elem_value_t *value = NULL;
    int iRet = ERR_OK;

    /* Set up element ID */
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    /* Create new control */
    snd_ctl_elem_info_set_id(info, id);
    if (BOOL_IS_TRUE(bElementIsSwitch)) {
        iRet = snd_ctl_add_boolean_elem_set(ctl, info, 1, 1);
    } else {
        iRet = snd_ctl_add_integer_elem_set(ctl, info, 1, 1, VOL_MIN, VOL_MAX, 1);
    }
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error("Failed to create control '%s': %s", name, snd_strerror(iRet));
        goto cleanup;
    }

    /* Get the assigned element number.
     * Look the element up again by name: when a 32-bit userland runs on a
     * 64-bit kernel (e.g. Volumio on Raspberry Pi) the compat ioctl path does
     * not copy the assigned numid back, so the id in 'info' still says 0.
     */
    snd_ctl_elem_id_set_numid(id, 0);
    snd_ctl_elem_info_set_id(info, id);
    iRet = snd_ctl_elem_info(ctl, info);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot look up created control '%s': %s",
            name,
            snd_strerror(iRet)
        );
        goto cleanup;
    }
    snd_ctl_elem_info_get_id(info, id);
    log_verbose(
        "Created control element '%s' with numid '%u'",
        name,
        snd_ctl_elem_id_get_numid(id)
    );

    /* Set the initial value */
    snd_ctl_elem_value_alloca(&value);
    snd_ctl_elem_value_set_id(value, id);
    if (BOOL_IS_TRUE(bElementIsSwitch)) {
        snd_ctl_elem_value_set_boolean(value, 0, DEFAULT_VOLUME_ENABLED);
    } else {
        snd_ctl_elem_value_set_integer(value, 0, DEFAULT_VOLUME_VALUE);
    }
    iRet = snd_ctl_elem_write(ctl, value);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Failed to set initial value of '%s': %s",
            name,
            snd_strerror(iRet)
        );
        goto cleanup;
    }

    /* Unlock the control so other processes can write to it */
    iRet = snd_ctl_elem_unlock(ctl, id);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error("Unlock of '%s' failed: %s", name, snd_strerror(iRet));
        goto cleanup;
    }

    iRet = ERR_OK;

cleanup:
    if (ERR_CODE_IS_ERROR(iRet)) {
        /* NOTE: call the snd_ctl_elem_*_free() only in case of errors,
         *       otherwise the removal of the control elements
         *       during the cleanup of the main()
         *       will produce a double-free! */
        if (value) {
            snd_ctl_elem_value_free(value);
        }
        if (info) {
            snd_ctl_elem_info_free(info);
        }
    }
    return iRet;
}

/* read the value of an ALSA user-defined control element */
static int read_alsa_udce_value(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *id,
    snd_ctl_elem_value_t **pValue
) {
    int iRet = ERR_OK;
    if (pValue == NULL) {
        return ERR_ERROR_GENERIC;
    }
    snd_ctl_elem_value_alloca(pValue);
    if (*pValue == NULL) {
        return ERR_ERROR_GENERIC;
    }
    snd_ctl_elem_value_set_id(*pValue, id);
    iRet = snd_ctl_elem_read(ctl, *pValue);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        return ERR_ERROR_GENERIC;
    }
    return ERR_OK;
}

/* read the integer value of an ALSA user-defined control element */
static int read_alsa_udce_integer_value(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *id,
    int *piValue
) {
    int iRet = ERR_OK;

    snd_ctl_elem_value_t *value = NULL;
    iRet = read_alsa_udce_value(ctl, id, &value);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Unable to read ALSA user-defined control element value (integer)");
        return iRet;
    }

    /* idx=0 is the index of the value in the control element */
    *piValue = snd_ctl_elem_value_get_integer(value, 0);

    log_verbose("Read integer value %d", *piValue);
    return iRet;
}

/* read the boolean value of an ALSA user-defined control element */
static int read_alsa_udce_boolean_value(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *id,
    int *pbValue
) {
    int iRet = ERR_OK;

    snd_ctl_elem_value_t *value = NULL;
    iRet = read_alsa_udce_value(ctl, id, &value);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Unable to read ALSA user-defined control element value (boolean)");
        return iRet;
    }

    /* idx=0 is the index of the value in the control element */
    *pbValue = (snd_ctl_elem_value_get_boolean(value, 0) ? BOOL_TRUE : BOOL_FALSE);

    log_verbose("Read boolean value %d", *pbValue);
    return iRet;
}

/* Does a control event refer to the given element? */
static int event_matches(snd_ctl_event_t *event, snd_ctl_elem_id_t *id) {
    unsigned int numid = snd_ctl_elem_id_get_numid(id);

    if (numid != 0) {
        return snd_ctl_event_elem_get_numid(event) == numid;
    }
    return (
        (snd_ctl_event_elem_get_interface(event) == snd_ctl_elem_id_get_interface(id)) &&
        (strcmp(snd_ctl_event_elem_get_name(event), snd_ctl_elem_id_get_name(id)) == 0)
    );
}


/* *************************************************************************
 * MIDI helpers
 * ************************************************************************* */

/* Find MIDI port for the card */
static int find_midi_port(int card, char *port, size_t port_len) {
    int iRet = ERR_OK;

    /* First, try with the subdevice: hw:$card,0,0 (e.g., "hw:0,0,0").
     * Note that char *port is both input and output argument. */
    snprintf(port, port_len, "hw:%d,0,0", card);

    /* Verify the port exists by trying to open it */
    snd_rawmidi_t *midi = NULL;
    iRet = snd_rawmidi_open(NULL, &midi, port, SND_RAWMIDI_NONBLOCK);

    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        /* Unable to find the MIDI port with the subdevice.
         * As a fallback, try without the subdevice:
         * hw:$card,0 (e.g., "hw:0,0"). */
        snprintf(port, port_len, "hw:%d,0", card);
        iRet = snd_rawmidi_open(NULL, &midi, port, SND_RAWMIDI_NONBLOCK);
    }

    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        /* Unable to find the MIDI port, return error */
        return ERR_ERROR_GENERIC;
    }

    /* MIDI port found, return OK */
    snd_rawmidi_close(midi);
    return ERR_OK;
}

/* Convert the given volume value (in [VOL_MIN, VOL_MAX])
 * to a dB value (in [min_db, max_db]), with a linear mapping. */
static double convert_volume_control_value_to_db(int iVolume) {
    return goConfiguration.min_db +
        ((goConfiguration.max_db - goConfiguration.min_db) * iVolume) / VOL_MAX;
}

/* Convert the given volume value in dB to the corresponding two bytes
 * that can be sent to the RME ADI-2 device via MIDI SysEx to set that volume. */
static void convert_db_value_to_sysex_bytes(
    double dValueDB,
    unsigned char *x,
    unsigned char *y
) {
    int raw = (int)(dValueDB * 10) + 4096;

    /* Ensure the raw value is in the allowed range */
    if (raw < RME_VALUE_VOLUME_MIN) {
        /* corresponding to RME_VALUE_VOLUME_MIN_DB */
        raw = RME_VALUE_VOLUME_MIN;
    }
    if (raw > RME_VALUE_VOLUME_MAX) {
        /* corresponding to RME_VALUE_VOLUME_MAX_DB */
        raw = RME_VALUE_VOLUME_MAX;
    }

    /* assign to the two byte output arguments */
    *x = (raw >> 7) & 0x1F;
    *y = raw & 0x7F;
}

/* Send an absolute dB level to ADI-2 via MIDI SysEx */
static int send_db(snd_rawmidi_t *midi, double dValueDB) {
    unsigned char x, y;
    convert_db_value_to_sysex_bytes(dValueDB, &x, &y);

    /* Example: set -10.0 dB on line 1/2 on RME ADI 2/4 PRO SE

       F0 00 20 0D 73 02 1B 1F 1C F7
       -- -------- -- -- -- ----- --
       ^  ^        ^  ^  ^  ^     ^
       |  |        |  |  |  |     |
       |  |        |  |  |  |     +- SysEx end (0xF7)
       |  |        |  |  |  +- -10 db
       |  |        |  |  +- line 1/2 (0x1B)
       |  |        |  +- set (0x02)
       |  |        +- ADI 2/4 PRO SE (0x73)
       |  +- RME ID (0x00 0x20 0x0D)
       +- SysEx start (0xF0)

    */
    unsigned char sysex[] = {
        0xF0,                           /* SysEx start */
        0x00, 0x20, 0x0D,               /* RME manufacturer ID */
        goConfiguration.device_id,      /* Device ID */
        0x02,                           /* Command: set value */
        goConfiguration.output_param,   /* Parameter: output volume */
        x, y,                           /* Volume bytes */
        0xF7                            /* SysEx end */
    };

    log_verbose(
        "WRITE MIDI SysEx: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (volume: %.1f dB)",
        sysex[0], sysex[1], sysex[2], sysex[3], sysex[4],
        sysex[5], sysex[6], sysex[7], sysex[8], sysex[9],
        dValueDB
    );

    ssize_t written = snd_rawmidi_write(midi, sysex, sizeof(sysex));
    if (written < 0) {
        log_error("WRITE MIDI SysEx failed: %s", snd_strerror(written));
        return ERR_ERROR_GENERIC;
    }
    return ERR_OK;
}

/* Send control value as volume to ADI-2 */
static int send_volume(snd_rawmidi_t *midi, int iValue) {
    double dValueDB = 0;
    int iRet = ERR_OK;

    log_info("SET Volume: %d", iValue);
    dValueDB = convert_volume_control_value_to_db(iValue);
    iRet = send_db(midi, dValueDB);
    if (ERR_CODE_IS_OK(iRet)) {
        log_info("SET Volume: %.1f dB", dValueDB);
    } else {
        log_error("SET Volume failed: error %d", iRet);
    }
    return iRet;
}

/* Send mute/unmute command to ADI-2 via MIDI SysEx */
static int send_mute(snd_rawmidi_t *midi, unsigned char chRMEValueMute) {
    /* Example: mute line 1/2 on RME ADI 2/4 PRO SE

       F0 00 20 0D 73 02 1B 60 01 F7
       -- -------- -- -- -- -- -- --
       ^  ^        ^  ^  ^  ^  ^  ^
       |  |        |  |  |  |  |  |
       |  |        |  |  |  |  |  +- SysEx end (0xF7)
       |  |        |  |  |  |  +- mute on (0x01)
       |  |        |  |  |  +- mute parameter (0x60)
       |  |        |  |  +- line 1/2 (0x1B)
       |  |        |  +- set (0x02)
       |  |        +- ADI 2/4 PRO SE (0x73)
       |  +- RME ID (0x00 0x20 0x0D)
       +- SysEx start (0xF0)

    */
    unsigned char sysex[] = {
        0xF0,                           /* SysEx start */
        0x00, 0x20, 0x0D,               /* RME manufacturer ID */
        goConfiguration.device_id,      /* Device ID */
        0x02,                           /* Command: set value */
        goConfiguration.output_param,   /* Parameter: output volume */
        RME_PARAM_MUTE,                 /* Parameter: mute */
        chRMEValueMute,                 /* 0x00 = off (unmute), 0x01 = on (mute) */
        0xF7                            /* SysEx end */
    };

    log_verbose(
        "WRITE MIDI SysEx: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (mute: %s)",
        sysex[0], sysex[1], sysex[2], sysex[3], sysex[4],
        sysex[5], sysex[6], sysex[7], sysex[8], sysex[9],
        (chRMEValueMute == RME_VALUE_MUTE_ON ? "on" : "off")
    );

    ssize_t written = snd_rawmidi_write(midi, sysex, sizeof(sysex));
    if (written < 0) {
        log_error("WRITE MIDI SysEx failed: %s", snd_strerror(written));
        return ERR_ERROR_GENERIC;
    } else {
        log_info(
            "SET Mute: %s",
            (chRMEValueMute == RME_VALUE_MUTE_ON ? "on" : "off")
        );
    }
    return ERR_OK;
}


/* *************************************************************************
 * Monitor Loop
 * ************************************************************************* */

/* Signal handler for the monitor loop */
static void monitor_loop_signal_handler(int sig) {
    (void)sig;
    gbServiceIsRunning = (sig_atomic_t)BOOL_FALSE;
}


/* Apply the current state (volume, mute) to the MIDI device */
static void apply_state(snd_rawmidi_t *midi, int iVolume, int bEnabled) {
    /* if the validation of the values fails, return immediately without changing the state */
    if ((iVolume < VOL_MIN) || (iVolume > VOL_MAX)) {
        log_verbose("Invalid value '%d' for the volume, ignoring it", iVolume);
        return;
    }
    if ((bEnabled != BOOL_TRUE) && (bEnabled != BOOL_FALSE)) {
        log_verbose("Invalid value '%d' for the mute switch, ignoring it", bEnabled);
        return;
    }

    /* NOTE: here we ignore errors from the send_() functions */
    if (BOOL_IS_TRUE(bEnabled)) {
        send_mute(midi, RME_VALUE_MUTE_OFF);
        send_volume(midi, iVolume);
    } else {
        send_mute(midi, RME_VALUE_MUTE_ON);
    }
}

/* Monitor loop (main service loop) */
static int monitor_loop(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *vol_id,
    snd_ctl_elem_id_t *sw_id,
    snd_rawmidi_t *midi
) {
    int bEnabled = DEFAULT_VOLUME_ENABLED;
    int bEnabledPrevious = bEnabled;
    int iRet = ERR_OK;
    int iVolume = DEFAULT_VOLUME_VALUE;
    int iVolumePrevious = iVolume;

    /* Subscribe to control events */
    iRet = snd_ctl_subscribe_events(ctl, 1);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error("Cannot subscribe to events: %s", snd_strerror(iRet));
        return iRet;
    }

    /* Get poll descriptors */
    int count = snd_ctl_poll_descriptors_count(ctl);
    if (count <= 0) {
        log_error("Invalid poll descriptor count: %d", count);
        return ERR_ERROR_GENERIC;
    }
    struct pollfd pfd[count];
    snd_ctl_poll_descriptors(ctl, pfd, count);

    /* Send initial state */
    /* TODO implement a real MIDI SysEx read of mute and volume state */
    apply_state(midi, iVolume, bEnabled);
    iVolumePrevious = iVolume;
    bEnabledPrevious = bEnabled;

    log_info(
        "Monitoring ALSA user-defined control elements '%s' and '%s' for changes...",
        CONTROL_NAME,
        SWITCH_NAME
    );

    while (BOOL_IS_TRUE(gbServiceIsRunning)) {
        iRet = poll(pfd, count, -1);  /* Block until event or signal */
        if (iRet < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("Poll error: %s", strerror(errno));
            break;
        }

        /* Check poll result */
        unsigned short revents;
        snd_ctl_poll_descriptors_revents(ctl, pfd, count, &revents);
        if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
            /* The card went away (device unplugged or powered off).
             * Exit so the descriptors are released, the kernel can free the
             * card slot, and systemd can restart us when the device returns.
             */
            log_error("Sound card disconnected");
            return -1;
        }
        if (!(revents & POLLIN))
            continue;

        /* Read events */
        snd_ctl_event_t *event;
        snd_ctl_event_alloca(&event);

        while (snd_ctl_read(ctl, event) > 0) {
            if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
                continue;

            /* Check if it's one of our controls */
            if (!event_matches(event, vol_id) && !event_matches(event, sw_id))
                continue;

            /* Read new state */
            iRet = read_alsa_udce_integer_value(ctl, vol_id, &iVolume);
            if (ERR_CODE_IS_ERROR(iRet)) {
                log_error("Unable to read the value of the volume");
                return iRet;
            }
            iRet = read_alsa_udce_boolean_value(ctl, sw_id, &bEnabled);
            if (ERR_CODE_IS_ERROR(iRet)) {
                log_error("Unable to read the value of the mute switch");
                return iRet;
            }
            if ((iVolume != iVolumePrevious) || (bEnabled != bEnabledPrevious)) {
                apply_state(midi, iVolume, bEnabled);
                iVolumePrevious = iVolume;
                bEnabledPrevious = bEnabled;
            }

            /* Process one event per poll, then check running flag */
            break;
        }
    }

    return ERR_OK;
}


/* *************************************************************************
 * CLI Helpers
 * ************************************************************************* */

/* return true if the given deviceID is supported */
static int validate_device_id(int deviceID) {
    /* it suffices to check the ..._FAMILIY_... codes */
    return (
        (deviceID == RME_DEVICE_FAMILY_ADI2_DAC) ||
        (deviceID == RME_DEVICE_FAMILY_ADI2_PRO) ||
        (deviceID == RME_DEVICE_FAMILY_ADI24_PRO)
    ) ? BOOL_TRUE : BOOL_FALSE;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Creates an ALSA mixer control (\"ADI2\": playback volume + switch)\n"
           "for RME ADI-2 hardware volume and mute.\n\n");
    printf("Options:\n");
    printf("  -c, --card NAME             Sound card name to search for (default: %s)\n", DEFAULT_CARD_NAME);
    printf("  -C, --card-num N            Use card number N directly\n");
    printf("  -m, --midi PORT             MIDI port (default: auto-detect)\n");
    printf("  -d, --device ID             RME device ID: 0x71=ADI-2 DAC, 0x72=ADI-2 Pro, 0x73=ADI-2/4 Pro (default: 0x%x)\n", DEFAULT_DEVICE_ID);
    printf("  -o, --output TYPE           Output: line or phones (default: line)\n");
    printf("      --min-db DB             Minimum dB (0%% volume) (default: %.1f)\n", goConfiguration.min_db);
    printf("      --max-db DB             Maximum dB (100%% volume) (default: %.1f)\n", goConfiguration.max_db);
    printf("      --no-uninstall-on-exit  If specified, do not uninstall ADI2 controls when the program exits\n");
    printf("      --uninstall-only        Remove the ADI2 mixer controls from the card and exit\n");
    printf("  -v, --verbose               Verbose output\n");
    printf("  -h, --help                  Show this help\n");
    printf("\nExample:\n");
    printf("  %s --card ADI-2 --device 0x73 --output line --max-db -20\n", prog);
}

static int parse_args(int argc, char *argv[]) {
    static struct option long_opts[] = {
        {"card",                 required_argument, 0, 'c'},
        {"card-num",             required_argument, 0, 'C'},
        {"midi",                 required_argument, 0, 'm'},
        {"device",               required_argument, 0, 'd'},
        {"output",               required_argument, 0, 'o'},
        {"min-db",               required_argument, 0, 'n'},
        {"max-db",               required_argument, 0, 'x'},
        {"no-uninstall-on-exit", no_argument,       0, 'U'},
        {"uninstall",            no_argument,       0, 'u'},
        {"verbose",              no_argument,       0, 'v'},
        {"help",                 no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:C:m:d:o:n:x:uUvh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'c':
            strncpy(goConfiguration.card_name, optarg, sizeof(goConfiguration.card_name) - 1);
            break;
        case 'C':
            goConfiguration.card_num = atoi(optarg);
            break;
        case 'm':
            strncpy(goConfiguration.midi_port, optarg, sizeof(goConfiguration.midi_port) - 1);
            break;
        case 'd':
            goConfiguration.device_id = (int)strtol(optarg, NULL, 0);
            if (BOOL_IS_FALSE(validate_device_id(goConfiguration.device_id))) {
                log_error("Invalid device ID: 0x%02X", goConfiguration.device_id);
                return EXIT_ERROR_USAGE_BAD_VALUE;
            }
            break;
        case 'o':
            if (strcmp(optarg, "line") == 0) {
                goConfiguration.output_param = RME_PARAM_LINE;
            } else if (strcmp(optarg, "phones") == 0) {
                goConfiguration.output_param = RME_PARAM_PHONES;
            } else {
                log_error("Invalid output: %s (use 'line' or 'phones')", optarg);
                return EXIT_ERROR_USAGE_BAD_VALUE;
            }
            break;
        case 'n':
            goConfiguration.min_db = atof(optarg);
            break;
        case 'x':
            goConfiguration.max_db = atof(optarg);
            break;
        case 'u':
            goConfiguration.bUninstallOnly = BOOL_TRUE;
            break;
        case 'U':
            goConfiguration.bUninstallOnExit = BOOL_FALSE;
            break;
        case 'v':
            gbLogVerbose = BOOL_TRUE;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(EXIT_OK);
        default:
            return EXIT_ERROR_USAGE_BAD_OPTION;
        }
    }

    return ERR_OK;
}


/* *************************************************************************
 * CLI Main
 * ************************************************************************* */

int main(int argc, char *argv[]) {
    snd_ctl_t *ctl = NULL;
    snd_rawmidi_t *midi = NULL;
    snd_ctl_elem_id_t *vol_id;
    snd_ctl_elem_id_t *sw_id;
    char card_hw[32] = "";
    int iRet = ERR_OK;
    int iExitCode = EXIT_OK;

    iRet = parse_args(argc, argv);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot parse command line arguments: exit error %d", iRet);
        print_usage(argv[0]);
        iExitCode = iRet;
        goto cleanup;
    }

    snd_ctl_elem_id_alloca(&vol_id);
    snd_ctl_elem_id_alloca(&sw_id);

    /* Set up signal handlers (no SA_RESTART so poll() returns EINTR) */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = monitor_loop_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Find card */
    if (goConfiguration.card_num < 0) {
        goConfiguration.card_num = find_card_by_name(goConfiguration.card_name);
        if (goConfiguration.card_num < 0) {
            log_error(
                "Cannot find a card with '%s' in its name",
                goConfiguration.card_name
            );
            iExitCode = EXIT_ERROR_CANNOT_FIND_CARD;
            goto cleanup;
        }
    }
    log_info("Using card '%d'", goConfiguration.card_num);

    /* Open card */
    snprintf(card_hw, sizeof(card_hw), "hw:%d", goConfiguration.card_num);
    iRet = snd_ctl_open(&ctl, card_hw, 0);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error("Cannot open card '%s': %s", card_hw, snd_strerror(iRet));
        iExitCode = EXIT_ERROR_CANNOT_OPEN_CARD;
        goto cleanup;
    }
    log_info("Opened card '%s'", card_hw);

    if (BOOL_IS_TRUE(goConfiguration.bUninstallOnly)) {
        /* Uninstall-only mode: go to the service clean-up immediately */
        log_verbose("Uninstall-only mode: exit immediately");
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Normal mode: ensure no user-defined control elements are defined,
     * ignoring any errors that might occur trying to remove them,
     * for example because no such user-defined control elements
     * survived a previous execution of the service. */
    iRet = uninstall_all_alsa_udce_s(ctl, BOOL_TRUE);

    /* Find MIDI port */
    if (goConfiguration.midi_port[0] == '\0') {
        iRet = find_midi_port(
            goConfiguration.card_num,
            goConfiguration.midi_port,
            sizeof(goConfiguration.midi_port)
        );
        if (ERR_CODE_IS_ERROR(iRet)) {
            log_error("Cannot find a MIDI port for card '%d'", goConfiguration.card_num);
            iExitCode = EXIT_ERROR_CANNOT_FIND_MIDI_PORT;
            goto cleanup;
        }
    }
    log_info("Using MIDI port '%s'", goConfiguration.midi_port);

    /* Open MIDI port */
    iRet = snd_rawmidi_open(NULL, &midi, goConfiguration.midi_port, 0);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot open MIDI port '%s': %s",
            goConfiguration.midi_port,
            snd_strerror(iRet)
        );
        iExitCode = EXIT_ERROR_CANNOT_OPEN_MIDI_PORT;
        goto cleanup;
    }

    /* Create user-defined control elements: volume first, then switch */
    iRet = install_control_element(ctl, vol_id, CONTROL_NAME, BOOL_FALSE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add volume control '%s'", CONTROL_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    iRet = install_control_element(ctl, sw_id, SWITCH_NAME, BOOL_TRUE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add mute switch control '%s'", SWITCH_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    /* Run the monitoring loop */
    log_info(
        "Start control loop for device ID '0x%02X' and output '%s'",
        goConfiguration.device_id,
        goConfiguration.output_param == RME_PARAM_LINE ? "line" : "phones"
    );
    iRet = monitor_loop(ctl, vol_id, sw_id, midi);
    log_info("Control loop exited with return code '%d'", iRet);
    if (ERR_CODE_IS_ERROR(iRet)) {
        iExitCode = EXIT_ERROR_GENERIC_IN_MONITOR_LOOP;
        goto cleanup;
    }

    /* Service exited successfully */
    iExitCode = EXIT_OK;

cleanup:
    log_verbose("Cleaning service up ...");

    /* Uninstall user-defined control elements */
    if (BOOL_IS_TRUE(goConfiguration.bUninstallOnly)) {
        /* Uninstall-only mode (capturing errors) */
        iRet = uninstall_all_alsa_udce_s(ctl, BOOL_FALSE);
        if (ERR_CODE_IS_ERROR(iRet)) {
            log_error("An error occurred while uninstalling user-defined control elements");
            if (iExitCode == EXIT_OK) {
                /* in case of error, overwrite the previous "OK" exit code */
                iExitCode = EXIT_ERROR_CANNOT_UNINSTALL;
            }
        }
    } else {
        /* Normal-mode: check the "uninstall on exit" flag */
        if (BOOL_IS_TRUE(goConfiguration.bUninstallOnExit)) {
            /* Uninstall (ignoring errors) */
            uninstall_all_alsa_udce_s(ctl, BOOL_TRUE);
        } else {
            log_info("Uninstall-only mode: skip uninstalling user-defined control elements");
        }
    }

    /* Close objects */
    if (midi) {
        snd_rawmidi_close(midi);
        log_verbose("Closed raw MIDI object");
    }
    if (ctl) {
        snd_ctl_close(ctl);
        log_verbose("Closed ALSA sound object");
    }

    log_verbose("Cleaning service up ... done");
    log_info("Cleaned service up");

    /* Log exit code and return it */
    log_info("Service exited with code '%d'", iExitCode);
    return iExitCode;
}
