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

/* Flag for verbose log */
static int gbLogVerbose = BOOL_FALSE;

/* Flag tracking whether the service is running or not */
static volatile sig_atomic_t gbServiceIsRunning = (sig_atomic_t)BOOL_TRUE;

/* Service configuration */
static struct {
    int card_number;
    char card_hw[CARD_HW_MAX_LENGTH];
    char device_name[DEVICE_NAME_MAX_LENGTH];
    int device_id;
    char midi_device[MIDI_DEVICE_MAX_LENGTH];
    int output_param;
    double min_db;
    double max_db;
    int bListCards;
    int bPrintUsageOnly;
    int bUninstall;
    int bUninstallOnExit;
} goConfiguration = {
    .card_number = DEFAULT_CARD_NUMBER,
    .card_hw = DEFAULT_CARD_HW,
    .device_name = DEFAULT_DEVICE_NAME,
    .device_id = DEFAULT_DEVICE_ID,
    .midi_device = DEFAULT_MIDI_DEVICE,
    .output_param = RME_PARAM_LINE,
    .min_db = DEFAULT_VOLUME_MIN_DB,
    .max_db = DEFAULT_VOLUME_MAX_DB,
    .bListCards = DEFAULT_LIST_CARDS_ONLY,
    .bPrintUsageOnly = DEFAULT_PRINT_USAGE_ONLY,
    .bUninstall = DEFAULT_UNINSTALL_ONLY,
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

/* Print the list of cards as a table with colums (Index, ID, Name). */
static void print_cards() {
    int iCard = -1;
    char *pCardName;

    printf("Index\tID\tName\n");
    while ((snd_card_next(&iCard) >= 0) && (iCard >= 0)) {
        if (ERR_ALSA_CODE_IS_ERROR(snd_card_get_name(iCard, &pCardName))) {
            /* ignore error and continue looping over the available cards */
            continue;
        }
        if (pCardName == NULL) {
            continue;
        }
        printf("%d\thw:%d\t%s\n", iCard, iCard, pCardName);
        free(pCardName);
    }
}

/* Find the card number of a card whose name contains the given query string,
 * returning it.
 *
 * The first (lowest card number) non-negative match is returned.
 *
 * If no match is found, return -1. */
static int find_card_by_name(const char *pQuery) {
    int iCard = -1;
    char *pCardName;

    log_verbose("Find ALSA card by name matching query '%s'", pQuery);

    while ((snd_card_next(&iCard) >= 0) && (iCard >= 0)) {
        if (snd_card_get_name(iCard, &pCardName) < 0) {
            continue;
        }
        if (pCardName == NULL) {
            continue;
        }

        log_verbose(
            "Discovered ALSA card number '%d' with name '%s'",
            iCard,
            pCardName
        );

        /* NOTE: matching pQuery as substring of pCardName */
        if (strstr(pCardName, pQuery) != NULL) {
            log_verbose(
                "ALSA card number '%d' with name '%s' "
                "matches the query '%s': returning it",
                iCard,
                pCardName,
                pQuery
            );
            free(pCardName);
            return iCard;
        }

        /* no match: free pCardName and continue looping */
        free(pCardName);
    }

    /* no match found: log and return -1 to signal failure */
    log_verbose(
        "No ALSA card found whose name matches the query '%s'",
        pQuery
    );
    return -1;
}

/* Find the first card corresponding to the given device name and ID:
 * return true if found, otherwise return false. */
static int find_card_and_set_configuration(
    const char* pDeviceName,
    int iDeviceID
) {
    int iCardNumber = find_card_by_name(pDeviceName);
    if (iCardNumber < 0) {
        /* card not found */
        return BOOL_FALSE;
    }

    /* card found */
    goConfiguration.device_id = iDeviceID;
    goConfiguration.card_number = iCardNumber;
    strncpy(
        goConfiguration.device_name,
        pDeviceName,
        DEVICE_NAME_MAX_LENGTH
    );
    log_verbose(
        "Found ALSA card number '%d' for device ID '0x%02X' (%s)",
        iCardNumber,
        iDeviceID,
        pDeviceName
    );
    return BOOL_TRUE;
}

/* Partials for the various ADI-2 product families. */
static int find_card_and_set_configuration_rme_adi24_pro() {
    return find_card_and_set_configuration(
        DEVICE_NAME_RME_ADI24_PRO,
        RME_DEVICE_FAMILY_ADI24_PRO
    );
}

static int find_card_and_set_configuration_rme_adi2_pro() {
    return find_card_and_set_configuration(
        DEVICE_NAME_RME_ADI2_PRO,
        RME_DEVICE_FAMILY_ADI2_PRO
    );
}

static int find_card_and_set_configuration_rme_adi2_dac() {
    return find_card_and_set_configuration(
        DEVICE_NAME_RME_ADI2_DAC,
        RME_DEVICE_FAMILY_ADI2_DAC
    );
}

static int device_name_is_autodetect_or_rme_adi24_pro() {
    return (
        (strncmp(goConfiguration.device_name, DEVICE_NAME_AUTODETECT, DEVICE_NAME_MAX_LENGTH) == 0) ||
        (strncmp(goConfiguration.device_name, DEVICE_NAME_RME_ADI24_PRO, DEVICE_NAME_MAX_LENGTH) == 0)
    );
}

static int device_name_is_autodetect_or_rme_adi2_pro() {
    return (
        (strncmp(goConfiguration.device_name, DEVICE_NAME_AUTODETECT, DEVICE_NAME_MAX_LENGTH) == 0) ||
        (strncmp(goConfiguration.device_name, DEVICE_NAME_RME_ADI2_PRO, DEVICE_NAME_MAX_LENGTH) == 0)
    );
}

static int device_name_is_autodetect_or_rme_adi2_dac() {
    return (
        (strncmp(goConfiguration.device_name, DEVICE_NAME_AUTODETECT, DEVICE_NAME_MAX_LENGTH) == 0) ||
        (strncmp(goConfiguration.device_name, DEVICE_NAME_RME_ADI2_DAC, DEVICE_NAME_MAX_LENGTH) == 0)
    );
}

/* Determine the device ID if the user did not specify it explicitly,
 * searching for a matching card using the device name.
 *
 * If the device name is "autodetect",
 * try all supported ADI-2 product families, in the appropriate order.
 *
 * The first matching card found (lowest card number) is set and used.
 *
 * Return true if a matching card is found; otherwise return false. */
static int ensure_device_id_is_set() {
    if (goConfiguration.device_id >= 0) {
        log_verbose(
            "Device ID '0x%02X' explicitly specified by the user",
            goConfiguration.device_id
        );
        return BOOL_TRUE;
    }

    log_verbose(
        "Find ALSA card number and device ID from device name '%s'",
        goConfiguration.device_name
    );

    if (
        BOOL_IS_TRUE(device_name_is_autodetect_or_rme_adi24_pro()) &&
        BOOL_IS_TRUE((find_card_and_set_configuration_rme_adi24_pro()))
    ) {
        return BOOL_TRUE;
    }

    if (
        BOOL_IS_TRUE(device_name_is_autodetect_or_rme_adi2_pro()) &&
        BOOL_IS_TRUE((find_card_and_set_configuration_rme_adi2_pro()))
    ) {
        return BOOL_TRUE;
    }

    if (
        BOOL_IS_TRUE(device_name_is_autodetect_or_rme_adi2_dac()) &&
        BOOL_IS_TRUE((find_card_and_set_configuration_rme_adi2_dac()))
    ) {
        return BOOL_TRUE;
    }

    return BOOL_FALSE;
}

/* Determine the card number if the user did not specify it explicitly,
 * searching for a card whose name matches the device ID.
 *
 * Return true if a matching card is found; otherwise return false. */
static int ensure_card_number_is_set() {
    int bCardFound = BOOL_FALSE;

    if (goConfiguration.card_number >= 0) {
        log_verbose(
            "ALSA card number '%d' explicitly specified by the user "
            "or already determined",
            goConfiguration.card_number
        );
        return BOOL_TRUE;
    }

    switch (goConfiguration.device_id) {
        case RME_DEVICE_FAMILY_ADI24_PRO:
            bCardFound = find_card_and_set_configuration_rme_adi24_pro();
            break;
        case RME_DEVICE_FAMILY_ADI2_PRO:
            bCardFound = find_card_and_set_configuration_rme_adi2_pro();
            break;
        case RME_DEVICE_FAMILY_ADI2_DAC:
            bCardFound = find_card_and_set_configuration_rme_adi2_pro();
            break;
        default:
            log_verbose(
                "goConfiguration.device_id has unsupported value '%d'",
                goConfiguration.device_id
            );
            break;
    }

    return bCardFound;
}

/* Determine the device ID and card number
 * if the user did not specify them.
 *
 * See the two helper functions:
 *
 * - ensure_device_id_is_set()
 * - ensure_card_number_is_set()
 *
 * for the details of the probing.
 *
 * Return ERR_OK if both the device ID and the card number
 * could be determined, otherwise return an error. */
static int ensure_device_id_and_card_number() {
    int bCardFound = BOOL_FALSE;

    bCardFound = ensure_device_id_is_set();
    if (BOOL_IS_FALSE(bCardFound)) {
        log_error(
            "Cannot find an ALSA card matching device name '%s'",
            goConfiguration.device_name
        );
        return ERR_ERROR_GENERIC;
    }

    bCardFound = ensure_card_number_is_set();
    if (BOOL_IS_FALSE(bCardFound)) {
        log_error(
            "Cannot find an ALSA card matching device id '0x%02X'",
            goConfiguration.device_id
        );
        return ERR_ERROR_GENERIC;
    }

    return ERR_OK;
}

/* Set the card hw from the card number */
static void set_card_hw() {
    snprintf(
        goConfiguration.card_hw,
        CARD_HW_MAX_LENGTH,
        "hw:%d",
        goConfiguration.card_number
    );
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

/* Check if an event is generated by the given control element */
static int is_event_from_udce(snd_ctl_event_t *event, snd_ctl_elem_id_t *id) {
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

/* Check if the given pMIDIDevice can be opened:
 * if so, return true; otherwise return false. */
static int is_valid_midi_device(const char *pMIDIDevice) {
    int iRet = ERR_OK;
    snd_rawmidi_t *midi = NULL;

    log_verbose("Check if MIDI device '%s' is valid", pMIDIDevice);
    /* NOTE: the flag SND_RAWMIDI_NONBLOCK prevents interfering
     *       with other processes that might be using the MIDI device
     *       being probed. */
    iRet = snd_rawmidi_open(NULL, &midi, pMIDIDevice, SND_RAWMIDI_NONBLOCK);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_verbose("MIDI device '%s' is NOT valid", pMIDIDevice);
        return BOOL_FALSE;
    }

    snd_rawmidi_close(midi);
    log_verbose("MIDI device '%s' is valid", pMIDIDevice);
    return BOOL_TRUE;
}

static int ensure_midi_device() {
    if (strncmp(goConfiguration.midi_device, MIDI_DEVICE_AUTODETECT, MIDI_DEVICE_MAX_LENGTH) != 0) {
        log_verbose(
            "Check if MIDI device '%s' for card number '%d' is valid",
            goConfiguration.midi_device,
            goConfiguration.card_number
        );

        if (BOOL_IS_TRUE(is_valid_midi_device(goConfiguration.midi_device))) {
            return ERR_OK;
        }

        return ERR_ERROR_GENERIC;
    }

    log_verbose(
        "Autodetect MIDI device for card number '%d'",
        goConfiguration.card_number
    );

    /* first, try with hw:X,i,j */
    for (int i = MIDI_DEVICE_PROBE_MIN_INDEX; i <= MIDI_DEVICE_PROBE_MAX_INDEX; i++) {
        for (int j = MIDI_DEVICE_PROBE_MIN_INDEX; j <= MIDI_DEVICE_PROBE_MAX_INDEX; j++) {
            snprintf(goConfiguration.midi_device, MIDI_DEVICE_MAX_LENGTH, "hw:%d,%d,%d", goConfiguration.card_number, i, j);
            if (BOOL_IS_TRUE(is_valid_midi_device(goConfiguration.midi_device))) {
                return ERR_OK;
            }
        }
    }

    /* second, try with just hw:X,i */
    for (int i = MIDI_DEVICE_PROBE_MIN_INDEX; i <= MIDI_DEVICE_PROBE_MAX_INDEX; i++) {
        snprintf(goConfiguration.midi_device, MIDI_DEVICE_MAX_LENGTH, "hw:%d,%d", goConfiguration.card_number, i);
        if (BOOL_IS_TRUE(is_valid_midi_device(goConfiguration.midi_device))) {
            return ERR_OK;
        }
    }

    /* third, try with just hw:X */
    snprintf(goConfiguration.midi_device, MIDI_DEVICE_MAX_LENGTH, "hw:%d", goConfiguration.card_number);
    if (BOOL_IS_TRUE(is_valid_midi_device(goConfiguration.midi_device))) {
        return ERR_OK;
    }

    return ERR_ERROR_GENERIC;
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
    /* If the validation of the values fails,
     * return immediately without changing the state */
    /* NOTE: for unknown reasons, sometimes the first time
     *       the volume value is read, it returns an invalid value */
    if ((iVolume < VOL_MIN) || (iVolume > VOL_MAX)) {
        log_verbose(
            "Invalid value '%d' for the volume, ignoring it",
            iVolume
        );
        return;
    }
    /* Check explicitly != BOOL_TRUE, BOOL_FALSE instead of using
     * BOOL_IS_* macros, to ensure only the canonical values are read */
    if ((bEnabled != BOOL_TRUE) && (bEnabled != BOOL_FALSE)) {
        log_verbose(
            "Invalid value '%d' for the mute switch, ignoring it",
            bEnabled
        );
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
    int iCount = 0;
    int iRet = ERR_OK;
    int bEnabled = DEFAULT_VOLUME_ENABLED;
    int iVolume = DEFAULT_VOLUME_VALUE;
    int bEnabledLast = bEnabled;
    int iVolumeLast = iVolume;
    unsigned short usRevents = 0;

    /* Subscribe to control events */
    iRet = snd_ctl_subscribe_events(ctl, 1);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error("Cannot subscribe to events: %s", snd_strerror(iRet));
        return ERR_ERROR_GENERIC;
    }

    /* Get poll descriptors */
    iCount = snd_ctl_poll_descriptors_count(ctl);
    if (iCount <= 0) {
        log_error("Invalid poll descriptor count: %d", iCount);
        return ERR_ERROR_GENERIC;
    }
    struct pollfd pfd[iCount];
    snd_ctl_poll_descriptors(ctl, pfd, iCount);

    /* Send initial state */
    /* TODO implement a real MIDI SysEx read of mute and volume state */
    apply_state(midi, iVolume, bEnabled);
    iVolumeLast = iVolume;
    bEnabledLast = bEnabled;

    log_info(
        "Monitoring ALSA user-defined control elements "
        "'%s' and '%s' for changes...",
        CONTROL_NAME,
        SWITCH_NAME
    );

    /* gbServiceIsRunning can be set to false by the signal handler,
     * interrupting the monitor loop */
    while (BOOL_IS_TRUE(gbServiceIsRunning)) {

        /* Wait for the next event or signal */
        iRet = poll(pfd, iCount, -1);
        if (iRet < 0) {
            /* poll returned an error: inspect errno */
            if (errno == EINTR) {
                /* system call poll interrupted by a signal:
                 * evaluates the loop condition again  */
                continue;
            }
            /* in all other cases (EFAULT, EINVAL, ENOMEM), error out */
            log_error("Event poll error: %s", strerror(errno));
            return ERR_ERROR_EVENT_POLL;
        }

        /* Check poll result */
        snd_ctl_poll_descriptors_revents(ctl, pfd, iCount, &usRevents);
        if (usRevents & (POLLERR | POLLHUP | POLLNVAL)) {
            /* The ALSA card was disconnected and/or powered off:
             * error out so that the service terminates
             * the current execution.
             * If run by systemd, a new execution can be attempted,
             * in case the ALSA card returns available later. */
            log_error("The ALSA card was disconnected");
            return ERR_ERROR_SOUND_CARD_DISCONNECTED;
        }
        if (!(usRevents & POLLIN)) {
            /* no data to be read:
             * evaluates the loop condition again  */
            continue;
        }

        /* Read events */
        snd_ctl_event_t *event;
        snd_ctl_event_alloca(&event);

        while (snd_ctl_read(ctl, event) > 0) {
            /* Check if the event is from a control element */
            if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM) {
                /* evaluate next event */
                continue;
            }

            /* Check if the event belongs to one of the UDCEs */
            if (
                BOOL_IS_FALSE(is_event_from_udce(event, vol_id)) &&
                BOOL_IS_FALSE(is_event_from_udce(event, sw_id))
            ) {
                /* evaluate next event */
                continue;
            }

            /* Read state */
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

            /* If the state changed, apply it */
            if ((iVolume != iVolumeLast) || (bEnabled != bEnabledLast)) {
                apply_state(midi, iVolume, bEnabled);
                iVolumeLast = iVolume;
                bEnabledLast = bEnabled;
            }
        } /* end of inner while (snd_clt_read(ctl, event) > 0) */

    } /* end of outer while (BOOL_IS_TRUE(gbServiceIsRunning)) */

    return ERR_OK;
}


/* *************************************************************************
 * CLI Helpers
 * ************************************************************************* */

/* Return true if the given deviceID is supported, and false otherwise. */
static int validate_device_id(int deviceID) {
    /* it suffices to check the ..._FAMILIY_... codes */
    return (
        (deviceID == RME_DEVICE_FAMILY_ADI2_DAC) ||
        (deviceID == RME_DEVICE_FAMILY_ADI2_PRO) ||
        (deviceID == RME_DEVICE_FAMILY_ADI24_PRO)
    ) ? BOOL_TRUE : BOOL_FALSE;
}

/* Return true if the given pDeviceName is supported, and false otherwise. */
static int validate_device_name(const char* pDeviceName, size_t cbDeviceName) {
    return (
        (strncmp(pDeviceName, DEVICE_NAME_AUTODETECT, cbDeviceName) == 0) ||
        (strncmp(pDeviceName, DEVICE_NAME_RME_ADI2_DAC, cbDeviceName) == 0) ||
        (strncmp(pDeviceName, DEVICE_NAME_RME_ADI2_PRO, cbDeviceName) == 0) ||
        (strncmp(pDeviceName, DEVICE_NAME_RME_ADI24_PRO, cbDeviceName) == 0)
    ) ? BOOL_TRUE : BOOL_FALSE;
}

/* Return true if the given pMIDIDevice is supported, and false otherwise. */
static int validate_midi_device(const char* pMIDIDevice, size_t cbMIDIDevice) {
    /* NOTE: not checking if pMIDIDevice is in the form "hw:X[,Y[,Z]]"
     *       since the value might be provided by the user and
     *       it is not clear that that form is the only one accepted by ALSA. */
    return (
        (strncmp(pMIDIDevice, MIDI_DEVICE_AUTODETECT, cbMIDIDevice) == 0) ||
        ((strnlen(pMIDIDevice, cbMIDIDevice) > 0))
    ) ? BOOL_TRUE : BOOL_FALSE;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Creates ALSA mute and volume controls for RME ADI-2 devices.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -C, --card-number INTEGER   Use the ALSA card with the given index; if specified, overrides the card detected by --device-name\n");
    printf("  -d, --device-name STRING    RME device name: '%s', '%s', '%s', '%s' (default: '%s')\n",
            DEVICE_NAME_AUTODETECT,
            DEVICE_NAME_RME_ADI2_DAC,
            DEVICE_NAME_RME_ADI2_PRO,
            DEVICE_NAME_RME_ADI24_PRO,
            DEFAULT_DEVICE_NAME
    );
    printf("  -D, --device-id STRING      RME device ID: '0x%02X' (%s), '0x%02X' (%s), '0x%02X' (%s); if specified, overrides the device ID detected by --device-name\n",
            RME_DEVICE_FAMILY_ADI2_DAC,
            DEVICE_NAME_RME_ADI2_DAC,
            RME_DEVICE_FAMILY_ADI2_PRO,
            DEVICE_NAME_RME_ADI2_PRO,
            RME_DEVICE_FAMILY_ADI24_PRO,
            DEVICE_NAME_RME_ADI24_PRO
    );
    printf("  -h, --help                  Show this help\n");
    printf("      --list-cards            List all ALSA cards and exit\n");
    printf("  -M, --midi-device STRING    Use the specified MIDI device: 'autodetect' or 'hw:X[,Y[,Z]]'\n");
    printf("      --no-uninstall-on-exit  If specified, do not uninstall ADI2 controls when the program exits\n");
    printf("      --uninstall             Remove the ADI2 mixer controls from the card and exit\n");
    printf("  -v, --verbose               Verbose output\n");

    /* TODO continue from here */
    printf("  -o, --output TYPE           Output: line or phones (default: line)\n");
    printf("      --min-db DB             Minimum dB (0%% volume) (default: %.1f)\n", goConfiguration.min_db);
    printf("      --max-db DB             Maximum dB (100%% volume) (default: %.1f)\n", goConfiguration.max_db);
    printf("\n");
    printf("Examples:\n");
    printf("  %s --list-cards\n", prog);
    printf("  %s --verbose --device-name autodetect\n", prog);
    printf("  %s --card-index 5 --device-id 0x%02X --line12-min-db -25.0 --line12-max-db 0.0\n", prog, RME_DEVICE_FAMILY_ADI24_PRO);
}

static int parse_args(int argc, char *argv[]) {
    static struct option long_opts[] = {
        {"card-number",              required_argument, 0, 'C'},
        {"device-name",              required_argument, 0, 'd'},
        {"device-id",                required_argument, 0, 'D'},
        {"help",                     no_argument,       0, 'h'},
        {"midi-device",              required_argument, 0, 'M'},
        {"output",                   required_argument, 0, 'o'},
        {"list-cards",               no_argument,       0, 'W'},    /* the short form is not public */
        {"min-db",                   required_argument, 0, 'n'},    /* the short form is not public */
        {"max-db",                   required_argument, 0, 'x'},    /* the short form is not public */
        {"no-uninstall-on-exit",     no_argument,       0, 'U'},    /* the short form is not public */
        {"uninstall",                no_argument,       0, 'u'},    /* the short form is not public */
        {"verbose",                  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "C:d:D:hM:o:Wn:x:Uuv", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'C':
            goConfiguration.card_number = atoi(optarg);
            break;
        case 'd':
            strncpy(goConfiguration.device_name, optarg, DEVICE_NAME_MAX_LENGTH - 1);
            if (BOOL_IS_FALSE(validate_device_name(goConfiguration.device_name, DEVICE_NAME_MAX_LENGTH))) {
                log_error("Invalid device name: '%s'", goConfiguration.device_name);
                return EXIT_ERROR_USAGE_BAD_VALUE;
            }
            break;
        case 'D':
            goConfiguration.device_id = (int)strtol(optarg, NULL, 0);
            if (BOOL_IS_FALSE(validate_device_id(goConfiguration.device_id))) {
                log_error("Invalid device ID: 0x%02X", goConfiguration.device_id);
                return EXIT_ERROR_USAGE_BAD_VALUE;
            }
            break;
        case 'h':
            goConfiguration.bPrintUsageOnly = BOOL_TRUE;
            break;
        case 'M':
            strncpy(goConfiguration.midi_device, optarg, MIDI_DEVICE_MAX_LENGTH - 1);
            if (BOOL_IS_FALSE(validate_midi_device(goConfiguration.midi_device, MIDI_DEVICE_MAX_LENGTH))) {
                log_error("Invalid midi device: '%s'", goConfiguration.midi_device);
                return EXIT_ERROR_USAGE_BAD_VALUE;
            }
            break;
        case 'o':
            /* TODO remove this */
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
            goConfiguration.bUninstall = BOOL_TRUE;
            break;
        case 'U':
            goConfiguration.bUninstallOnExit = BOOL_FALSE;
            break;
        case 'v':
            gbLogVerbose = BOOL_TRUE;
            break;
        case 'W':
            goConfiguration.bListCards = BOOL_TRUE;
            break;
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
    snd_ctl_elem_id_t *vol_id = NULL;
    snd_ctl_elem_id_t *sw_id = NULL;
    int iRet = ERR_OK;
    int iExitCode = EXIT_OK;

    iRet = parse_args(argc, argv);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot parse command line arguments: exit error %d", iRet);
        print_usage(argv[0]);
        iExitCode = iRet;
        goto cleanup;
    }

    /* Print the usage and exit */
    if (BOOL_IS_TRUE(goConfiguration.bPrintUsageOnly)) {
        gbLogVerbose = BOOL_FALSE;
        print_usage(argv[0]);
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Print the list of cards and exit */
    if (BOOL_IS_TRUE(goConfiguration.bListCards)) {
        gbLogVerbose = BOOL_FALSE;
        print_cards();
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Set up signal handlers, without setting SA_RESTART
     * so that poll() returns EINTR when interrupted. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = monitor_loop_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Ensure that the device ID and card number are set,
     * possibly autodetecting them if not set by the user. */
    iRet = ensure_device_id_and_card_number();
    if (ERR_CODE_IS_ERROR(iRet)) {
        iExitCode = EXIT_ERROR_CANNOT_FIND_CARD;
        goto cleanup;
    }
    log_info("Using device ID '0x%02X'", goConfiguration.device_id);
    log_info("Using ALSA card '%d'", goConfiguration.card_number);

    /* Set the card hw from the card number */
    set_card_hw();

    /* Open card */
    iRet = snd_ctl_open(&ctl, goConfiguration.card_hw, 0);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot open ALSA card '%s': %s",
            goConfiguration.card_hw,
            snd_strerror(iRet)
        );
        iExitCode = EXIT_ERROR_CANNOT_OPEN_CARD;
        goto cleanup;
    }
    log_verbose("Opened ALSA card '%s'", goConfiguration.card_hw);

    if (BOOL_IS_TRUE(goConfiguration.bUninstall)) {
        /* Uninstall mode: go to the service clean-up immediately */
        log_verbose("Uninstall mode: exit immediately");
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Normal mode: ensure no user-defined control elements are defined,
     * ignoring any errors that might occur trying to remove them,
     * for example because no such user-defined control elements
     * survived a previous execution of the service. */
    iRet = uninstall_all_alsa_udce_s(ctl, BOOL_TRUE);

    /* Ensure that the MIDI device is set,
     * possibly autodetecting it if not set by the user. */
    iRet = ensure_midi_device();
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot find a valid MIDI device for ALSA card number '%d'",
            goConfiguration.card_number
        );
        iExitCode = EXIT_ERROR_CANNOT_FIND_MIDI_DEVICE;
        goto cleanup;
    }
    log_info("Using MIDI device '%s'", goConfiguration.midi_device);

    /* Open the MIDI device with flag 0 (default, blocking) */
    iRet = snd_rawmidi_open(NULL, &midi, goConfiguration.midi_device, 0);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot open MIDI device '%s': %s",
            goConfiguration.midi_device,
            snd_strerror(iRet)
        );
        iExitCode = EXIT_ERROR_CANNOT_OPEN_MIDI_DEVICE;
        goto cleanup;
    }

    /* Create user-defined control elements: volume first, then switch */
    snd_ctl_elem_id_alloca(&vol_id);
    iRet = install_control_element(ctl, vol_id, CONTROL_NAME, BOOL_FALSE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add volume control '%s'", CONTROL_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    snd_ctl_elem_id_alloca(&sw_id);
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
        switch (iRet) {
            case ERR_ERROR_EVENT_POLL:
                iExitCode = EXIT_ERROR_EVENT_POLL;
                break;
            case ERR_ERROR_SOUND_CARD_DISCONNECTED:
                iExitCode = EXIT_ERROR_SOUND_CARD_DISCONNECTED;
                break;
            default:
                iExitCode = EXIT_ERROR_GENERIC_IN_MONITOR_LOOP;
                break;
        }
        goto cleanup;
    }

    /* Service exited successfully */
    iExitCode = EXIT_OK;

cleanup:
    log_verbose("Cleaning service up ...");

    /* Uninstall user-defined control elements */
    if (BOOL_IS_TRUE(goConfiguration.bUninstall)) {
        if (ctl) {
            /* Uninstall mode (capturing errors) */
            iRet = uninstall_all_alsa_udce_s(ctl, BOOL_FALSE);
            if (ERR_CODE_IS_ERROR(iRet)) {
                log_error(
                    "An error occurred while uninstalling "
                    "user-defined control elements"
                );
                if (iExitCode == EXIT_OK) {
                    /* on error, overwrite the previous "OK" exit code */
                    iExitCode = EXIT_ERROR_CANNOT_UNINSTALL;
                }
            }
        }
    } else {
        /* Normal-mode: check the "uninstall on exit" flag */
        if (BOOL_IS_TRUE(goConfiguration.bUninstallOnExit)) {
            if (ctl) {
                /* Uninstall (ignoring errors) */
                uninstall_all_alsa_udce_s(ctl, BOOL_TRUE);
            }
        } else {
            log_info(
                "Skip uninstalling user-defined control elements on exit"
            );
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
    log_verbose("Cleaned service up");

    /* Log exit code and return it */
    log_verbose("Service exited with code '%d'", iExitCode);
    return iExitCode;
}
