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
    char szCardHw[CARD_HW_MAX_LENGTH];
    char szMIDIDevice[MIDI_DEVICE_MAX_LENGTH];
    char szDeviceName[DEVICE_NAME_MAX_LENGTH];
    int iCardNumber;
    int iDeviceID;
    double dLine12MinDB;
    double dLine12MaxDB;
    double dPhones34MinDB;
    double dPhones34MaxDB;
    int bListCards;
    int bListDeviceIDs;
    int bListDeviceNames;
    int bPrintUsageOnly;
    int bUninstall;
    int bUninstallOnExit;
} gpstConfiguration = {
    .szCardHw = DEFAULT_CARD_HW,
    .szMIDIDevice = DEFAULT_MIDI_DEVICE,
    .szDeviceName = DEFAULT_DEVICE_NAME,
    .iCardNumber = DEFAULT_CARD_NUMBER,
    .iDeviceID = DEFAULT_DEVICE_ID,
    .dLine12MinDB = DEFAULT_LINE12_VOLUME_MIN_DB,
    .dLine12MaxDB = DEFAULT_LINE12_VOLUME_MAX_DB,
    .dPhones34MinDB = DEFAULT_PHONES34_VOLUME_MIN_DB,
    .dPhones34MaxDB = DEFAULT_PHONES34_VOLUME_MAX_DB,
    .bListCards = DEFAULT_LIST_CARDS_ONLY,
    .bListDeviceIDs = DEFAULT_LIST_DEVICE_IDS_ONLY,
    .bListDeviceNames = DEFAULT_LIST_DEVICE_NAMES_ONLY,
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
    char *szCardName;

    printf("Index\tID\tName\n");
    while ((snd_card_next(&iCard) >= 0) && (iCard >= 0)) {
        if (ERR_ALSA_CODE_IS_ERROR(snd_card_get_name(iCard, &szCardName))) {
            /* ignore error and continue looping over the available cards */
            continue;
        }
        if (szCardName == NULL) {
            continue;
        }
        printf("%d\thw:%d\t%s\n", iCard, iCard, szCardName);
        free(szCardName);
    }
}

/* Find the card number of a card whose name contains the given query string,
 * returning it.
 *
 * The first (lowest card number) non-negative match is returned.
 *
 * If no match is found, return -1. */
static int find_card_by_name(const char *szQuery) {
    int iCard = -1;
    char *szCardName;

    log_verbose("Find ALSA card by name matching query '%s'", szQuery);

    while ((snd_card_next(&iCard) >= 0) && (iCard >= 0)) {
        if (snd_card_get_name(iCard, &szCardName) < 0) {
            continue;
        }
        if (szCardName == NULL) {
            continue;
        }

        log_verbose(
            "Discovered ALSA card number '%d' with name '%s'",
            iCard,
            szCardName
        );

        /* NOTE: matching szQuery as substring of szCardName */
        if (strstr(szCardName, szQuery) != NULL) {
            log_verbose(
                "ALSA card number '%d' with name '%s' "
                "matches the query '%s': returning it",
                iCard,
                szCardName,
                szQuery
            );
            free(szCardName);
            return iCard;
        }

        /* no match: free szCardName and continue looping */
        free(szCardName);
    }

    /* no match found: log and return -1 to signal failure */
    log_verbose(
        "No ALSA card found whose name matches the query '%s'",
        szQuery
    );
    return -1;
}

/* Find the first card corresponding to the given device name and ID:
 * return true if found, otherwise return false. */
static int find_card_and_set_configuration(
    const char *szDeviceName,
    int iDeviceID
) {
    int iCardNumber = find_card_by_name(szDeviceName);
    if (iCardNumber < 0) {
        /* card not found */
        return BOOL_FALSE;
    }

    /* card found */
    gpstConfiguration.iDeviceID = iDeviceID;
    gpstConfiguration.iCardNumber = iCardNumber;
    strncpy(
        gpstConfiguration.szDeviceName,
        szDeviceName,
        DEVICE_NAME_MAX_LENGTH
    );
    log_verbose(
        "Found ALSA card number '%d' for device ID '0x%02X' (%s)",
        iCardNumber,
        iDeviceID,
        szDeviceName
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
        (strncmp(gpstConfiguration.szDeviceName, DEVICE_NAME_AUTODETECT, DEVICE_NAME_MAX_LENGTH) == 0) ||
        (strncmp(gpstConfiguration.szDeviceName, DEVICE_NAME_RME_ADI24_PRO, DEVICE_NAME_MAX_LENGTH) == 0)
    );
}

static int device_name_is_autodetect_or_rme_adi2_pro() {
    return (
        (strncmp(gpstConfiguration.szDeviceName, DEVICE_NAME_AUTODETECT, DEVICE_NAME_MAX_LENGTH) == 0) ||
        (strncmp(gpstConfiguration.szDeviceName, DEVICE_NAME_RME_ADI2_PRO, DEVICE_NAME_MAX_LENGTH) == 0)
    );
}

static int device_name_is_autodetect_or_rme_adi2_dac() {
    return (
        (strncmp(gpstConfiguration.szDeviceName, DEVICE_NAME_AUTODETECT, DEVICE_NAME_MAX_LENGTH) == 0) ||
        (strncmp(gpstConfiguration.szDeviceName, DEVICE_NAME_RME_ADI2_DAC, DEVICE_NAME_MAX_LENGTH) == 0)
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
    if (gpstConfiguration.iDeviceID >= 0) {
        log_verbose(
            "Device ID '0x%02X' explicitly specified by the user",
            gpstConfiguration.iDeviceID
        );
        return BOOL_TRUE;
    }

    log_verbose(
        "Find ALSA card number and device ID from device name '%s'",
        gpstConfiguration.szDeviceName
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

    if (gpstConfiguration.iCardNumber >= 0) {
        log_verbose(
            "ALSA card number '%d' explicitly specified by the user "
            "or already determined",
            gpstConfiguration.iCardNumber
        );
        return BOOL_TRUE;
    }

    switch (gpstConfiguration.iDeviceID) {
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
                "gpstConfiguration.iDeviceID has unsupported value '%d'",
                gpstConfiguration.iDeviceID
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
            gpstConfiguration.szDeviceName
        );
        return ERR_ERROR_GENERIC;
    }

    bCardFound = ensure_card_number_is_set();
    if (BOOL_IS_FALSE(bCardFound)) {
        log_error(
            "Cannot find an ALSA card matching device id '0x%02X'",
            gpstConfiguration.iDeviceID
        );
        return ERR_ERROR_GENERIC;
    }

    return ERR_OK;
}

/* Set the card hw string from the card number */
static void set_card_hw_string() {
    snprintf(
        gpstConfiguration.szCardHw,
        CARD_HW_MAX_LENGTH,
        "hw:%d",
        gpstConfiguration.iCardNumber
    );
}

/* *************************************************************************
 * ALSA user-defined control elements helpers
 * ************************************************************************* */

/* Uninstall an ALSA user-defined control element (UDCE)
 * specified by name from the given card ctl. */
static int uninstall_alsa_udce_by_name(
    snd_ctl_t *ctl,
    const char *szName
) {
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, szName);
    int iRet = ERR_OK;

    iRet = snd_ctl_elem_remove(ctl, id);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_verbose("Unable to remove control with name '%s'", szName);
        return ERR_ERROR_GENERIC;
    }

    log_verbose("Removed control with name '%s'", szName);
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
    static const char *szUDCENames[] = {
        UDCE_LINE12_SWITCH_NAME,
        UDCE_LINE12_VOLUME_NAME,
        UDCE_PHONES34_SWITCH_NAME,
        UDCE_PHONES34_VOLUME_NAME
    };
    int iRemovedCount = 0;
    int iRet = ERR_OK;
    int iRetInner = ERR_OK;

    size_t cCount = sizeof(szUDCENames) / sizeof(szUDCENames[0]);
    for (size_t i = 0; i < cCount; i++) {
        iRetInner = uninstall_alsa_udce_by_name(ctl, szUDCENames[i]);
        if (ERR_CODE_IS_OK(iRetInner)) {
            iRemovedCount += 1;
        } else {
            iRet = ERR_ERROR_GENERIC;
        }
    }
    log_verbose(
        "Removed %d out of %d user-defined control element(s)",
        iRemovedCount,
        cCount
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

/* Install a new control (identified by id and szName) on the card ctl.
 * The control is a boolean switch (mute) if bElementIsSwitch is true,
 * otherwise it is an integer value (volume). */
static int install_control_element(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *id,
    const char *szName,
    int bElementIsSwitch
) {
    snd_ctl_elem_info_t *info = NULL;
    snd_ctl_elem_value_t *value = NULL;
    int iRet = ERR_OK;

    /* Set up element ID */
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, szName);
    snd_ctl_elem_info_set_id(info, id);

    /* Create new control */
    snd_ctl_elem_info_set_id(info, id);
    if (BOOL_IS_TRUE(bElementIsSwitch)) {
        iRet = snd_ctl_add_boolean_elem_set(ctl, info, 1, 1);
    } else {
        iRet = snd_ctl_add_integer_elem_set(ctl, info, 1, 1, VOL_MIN, VOL_MAX, 1);
    }
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Failed to create control '%s': %s",
            szName,
            snd_strerror(iRet)
        );
        goto cleanup;
    }
    snd_ctl_elem_id_set_numid(id, 0);
    snd_ctl_elem_info_set_id(info, id);
    iRet = snd_ctl_elem_info(ctl, info);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot find the created control '%s': %s",
            szName,
            snd_strerror(iRet)
        );
        goto cleanup;
    }
    snd_ctl_elem_info_get_id(info, id);
    log_verbose(
        "Created the control element '%s' with numid '%u'",
        szName,
        snd_ctl_elem_id_get_numid(id)
    );

    /* Set the initial value */
    snd_ctl_elem_value_alloca(&value);
    snd_ctl_elem_value_set_id(value, id);
    if (BOOL_IS_TRUE(bElementIsSwitch)) {
        snd_ctl_elem_value_set_boolean(value, 0, DEFAULT_LINE12_VOLUME_SWITCH_ENABLED);
    } else {
        snd_ctl_elem_value_set_integer(value, 0, DEFAULT_LINE12_VOLUME_VALUE);
    }
    iRet = snd_ctl_elem_write(ctl, value);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Failed to set initial value of '%s': %s",
            szName,
            snd_strerror(iRet)
        );
        goto cleanup;
    }

    /* Unlock the control so other processes can write to it */
    iRet = snd_ctl_elem_unlock(ctl, id);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Unlock of '%s' failed: %s",
            szName,
            snd_strerror(iRet)
        );
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
        log_error(
            "Unable to read ALSA user-defined control element value (integer)"
        );
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
        log_error(
            "Unable to read ALSA user-defined control element value (boolean)"
        );
        return iRet;
    }

    /* idx=0 is the index of the value in the control element */
    *pbValue = (snd_ctl_elem_value_get_boolean(value, 0) ? BOOL_TRUE : BOOL_FALSE);

    log_verbose("Read boolean value %d", *pbValue);
    return iRet;
}

/* Check if an event is generated by the given control element */
static int is_event_from_udce(snd_ctl_event_t *event, snd_ctl_elem_id_t *id) {
    unsigned int uNumID = snd_ctl_elem_id_get_numid(id);

    if (uNumID != 0) {
        return (snd_ctl_event_elem_get_numid(event) == uNumID) ? BOOL_TRUE : BOOL_FALSE;
    }

    return (
        (snd_ctl_event_elem_get_interface(event) == snd_ctl_elem_id_get_interface(id)) &&
        (strcmp(snd_ctl_event_elem_get_name(event), snd_ctl_elem_id_get_name(id)) == 0)
    ) ? BOOL_TRUE : BOOL_FALSE;
}


/* *************************************************************************
 * MIDI helpers
 * ************************************************************************* */

/* Check if the given szMIDIDevice can be opened:
 * if so, return true; otherwise return false. */
static int is_valid_midi_device(const char *szMIDIDevice) {
    int iRet = ERR_OK;
    snd_rawmidi_t *midi = NULL;

    log_verbose("Check if MIDI device '%s' is valid", szMIDIDevice);
    /* NOTE: the flag SND_RAWMIDI_NONBLOCK prevents interfering
     *       with other processes that might be using the MIDI device
     *       being probed. */
    iRet = snd_rawmidi_open(NULL, &midi, szMIDIDevice, SND_RAWMIDI_NONBLOCK);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_verbose("MIDI device '%s' is NOT valid", szMIDIDevice);
        return BOOL_FALSE;
    }

    snd_rawmidi_close(midi);
    log_verbose("MIDI device '%s' is valid", szMIDIDevice);
    return BOOL_TRUE;
}

static int ensure_midi_device() {
    if (
        strncmp(
            gpstConfiguration.szMIDIDevice,
            MIDI_DEVICE_AUTODETECT,
            MIDI_DEVICE_MAX_LENGTH
        ) != 0
    ) {
        log_verbose(
            "Check if MIDI device '%s' for card number '%d' is valid",
            gpstConfiguration.szMIDIDevice,
            gpstConfiguration.iCardNumber
        );

        if (BOOL_IS_TRUE(is_valid_midi_device(gpstConfiguration.szMIDIDevice))) {
            return ERR_OK;
        }

        return ERR_ERROR_GENERIC;
    }

    log_verbose(
        "Autodetect MIDI device for card number '%d'",
        gpstConfiguration.iCardNumber
    );

    /* first, try with hw:X,i,j */
    for (int i = MIDI_DEVICE_PROBE_MIN_INDEX; i <= MIDI_DEVICE_PROBE_MAX_INDEX; i++) {
        for (int j = MIDI_DEVICE_PROBE_MIN_INDEX; j <= MIDI_DEVICE_PROBE_MAX_INDEX; j++) {
            snprintf(
                gpstConfiguration.szMIDIDevice,
                MIDI_DEVICE_MAX_LENGTH,
                "hw:%d,%d,%d",
                gpstConfiguration.iCardNumber,
                i,
                j
            );
            if (BOOL_IS_TRUE(is_valid_midi_device(gpstConfiguration.szMIDIDevice))) {
                return ERR_OK;
            }
        }
    }

    /* second, try with just hw:X,i */
    for (int i = MIDI_DEVICE_PROBE_MIN_INDEX; i <= MIDI_DEVICE_PROBE_MAX_INDEX; i++) {
        snprintf(
            gpstConfiguration.szMIDIDevice,
            MIDI_DEVICE_MAX_LENGTH,
            "hw:%d,%d",
            gpstConfiguration.iCardNumber,
            i
        );
        if (BOOL_IS_TRUE(is_valid_midi_device(gpstConfiguration.szMIDIDevice))) {
            return ERR_OK;
        }
    }

    /* third, try with just hw:X */
    snprintf(
        gpstConfiguration.szMIDIDevice,
        MIDI_DEVICE_MAX_LENGTH,
        "hw:%d",
        gpstConfiguration.iCardNumber
    );
    if (BOOL_IS_TRUE(is_valid_midi_device(gpstConfiguration.szMIDIDevice))) {
        return ERR_OK;
    }

    return ERR_ERROR_GENERIC;
}

/* Convert the given volume value (in [VOL_MIN, VOL_MAX])
 * to a dB value (in [dLine12MinDB, dLine12MaxDB]), with a linear mapping. */
static double convert_volume_control_value_to_db(
    int iOutputID,
    int iVolume
) {
    if (iOutputID == UDCE_LINE12) {
        return gpstConfiguration.dLine12MinDB + ((gpstConfiguration.dLine12MaxDB - gpstConfiguration.dLine12MinDB) * iVolume) / VOL_MAX;
    }
    return gpstConfiguration.dPhones34MinDB + ((gpstConfiguration.dPhones34MaxDB - gpstConfiguration.dPhones34MinDB) * iVolume) / VOL_MAX;
}

/* Convert the given volume value in dB to the corresponding two bytes
 * that can be sent to the RME ADI-2 device via MIDI SysEx to set that volume. */
static void convert_db_value_to_sysex_bytes(
    double dValueDB,
    unsigned char *pchHigh,
    unsigned char *pchLow
) {
    int raw = (int)(dValueDB * 10) + 4096;

    /* Ensure the raw value is in the allowed range */
    if (raw < RME_VALUE_VOLUME_MIN) {
        /* corresponding to RME_VALUE_VOLUME_dLine12MinDB */
        raw = RME_VALUE_VOLUME_MIN;
    }
    if (raw > RME_VALUE_VOLUME_MAX) {
        /* corresponding to RME_VALUE_VOLUME_dLine12MaxDB */
        raw = RME_VALUE_VOLUME_MAX;
    }

    /* assign to the two byte output arguments */
    *pchHigh = (raw >> 7) & 0x1F;
    *pchLow = raw & 0x7F;
}

/* Send an absolute dB level to ADI-2 via MIDI SysEx */
static int send_db(
    snd_rawmidi_t *midi,
    unsigned char chOutput,
    double dValueDB
) {
    unsigned char chHigh = 0;
    unsigned char chLow = 0;
    convert_db_value_to_sysex_bytes(dValueDB, &chHigh, &chLow);

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
    unsigned char pchSysEx[] = {
        0xF0,                           /* SysEx start */
        0x00, 0x20, 0x0D,               /* RME manufacturer ID */
        gpstConfiguration.iDeviceID,      /* Device ID */
        0x02,                           /* Command: set value */
        chOutput,                       /* Parameter: output volume */
        chHigh, chLow,                  /* Volume bytes */
        0xF7                            /* SysEx end */
    };

    log_verbose(
        "WRITE MIDI SysEx: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (volume: %.1f dB)",
        pchSysEx[0], pchSysEx[1], pchSysEx[2], pchSysEx[3], pchSysEx[4],
        pchSysEx[5], pchSysEx[6], pchSysEx[7], pchSysEx[8], pchSysEx[9],
        dValueDB
    );

    ssize_t cbWritten = snd_rawmidi_write(midi, pchSysEx, sizeof(pchSysEx));
    if (cbWritten < 0) {
        log_error("WRITE MIDI SysEx failed: %s", snd_strerror(cbWritten));
        return ERR_ERROR_GENERIC;
    }
    return ERR_OK;
}

/* Send control value as volume to ADI-2 */
static int send_volume(
    snd_rawmidi_t *midi,
    int iOutputID,
    unsigned char chOutput,
    int iValue
) {
    double dValueDB = 0;
    int iRet = ERR_OK;

    log_info("SET Volume: %d", iValue);
    dValueDB = convert_volume_control_value_to_db(iOutputID, iValue);
    iRet = send_db(midi, chOutput, dValueDB);
    if (ERR_CODE_IS_OK(iRet)) {
        log_info("SET Volume: %.1f dB", dValueDB);
    } else {
        log_error("SET Volume failed: error %d", iRet);
    }
    return iRet;
}

/* Send mute/unmute command to ADI-2 via MIDI SysEx */
static int send_mute(
    snd_rawmidi_t *midi,
    unsigned char chOutput,
    unsigned char chRMEValueMute
) {
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
    unsigned char pchSysEx[] = {
        0xF0,                           /* SysEx start */
        0x00, 0x20, 0x0D,               /* RME manufacturer ID */
        gpstConfiguration.iDeviceID,      /* Device ID */
        0x02,                           /* Command: set value */
        chOutput,                       /* Parameter: output volume */
        RME_PARAM_MUTE,                 /* Parameter: mute */
        chRMEValueMute,                 /* 0x00 = off (unmute), 0x01 = on (mute) */
        0xF7                            /* SysEx end */
    };

    log_verbose(
        "WRITE MIDI SysEx: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (mute: %s)",
        pchSysEx[0], pchSysEx[1], pchSysEx[2], pchSysEx[3], pchSysEx[4],
        pchSysEx[5], pchSysEx[6], pchSysEx[7], pchSysEx[8], pchSysEx[9],
        (chRMEValueMute == RME_VALUE_MUTE_ON ? "on" : "off")
    );

    ssize_t cbWritten = snd_rawmidi_write(midi, pchSysEx, sizeof(pchSysEx));
    if (cbWritten < 0) {
        log_error("WRITE MIDI SysEx failed: %s", snd_strerror(cbWritten));
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
static void monitor_loop_signal_handler(int iSignal) {
    (void)iSignal;
    gbServiceIsRunning = (sig_atomic_t)BOOL_FALSE;
}


/* Apply the current state (volume, mute) to the MIDI device */
static void apply_state(
    snd_rawmidi_t *midi,
    int iOutputID,
    int iVolume,
    int bSwitch
) {
    unsigned char chOutput = 0;

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
    if ((bSwitch != BOOL_TRUE) && (bSwitch != BOOL_FALSE)) {
        log_verbose(
            "Invalid value '%d' for the switch, ignoring it",
            bSwitch
        );
        return;
    }

    switch (iOutputID) {
        case UDCE_LINE12:
            chOutput = RME_PARAM_LINE;
            break;
        case UDCE_PHONES34:
            chOutput = RME_PARAM_PHONES;
            break;
        default:
            log_verbose(
                "Invalid value '%d' for the output ID, ignoring it",
                iOutputID
            );
            return;
    }

    /* NOTE: here we ignore errors from the send_() functions */
    if (BOOL_IS_TRUE(bSwitch)) {
        send_mute(midi, chOutput, RME_VALUE_MUTE_OFF);
        send_volume(midi, iOutputID, chOutput, iVolume);
    } else {
        send_mute(midi, chOutput, RME_VALUE_MUTE_ON);
    }
}

/* Monitor loop (main service loop) */
static int monitor_loop(
    snd_ctl_t *ctl,
    snd_ctl_elem_id_t *pLine12VolumeID,
    snd_ctl_elem_id_t *pLine12SwitchID,
    snd_ctl_elem_id_t *pPhones34VolumeID,
    snd_ctl_elem_id_t *pPhones34SwitchID,
    snd_rawmidi_t *midi
) {
    int iCount = 0;
    int iRet = ERR_OK;
    unsigned short usRevents = 0;

    int bEventFromLine12 = BOOL_FALSE;
    int bEventFromPhones34 = BOOL_FALSE;

    /* line 1/2 output */
    int bLine12Switch = DEFAULT_LINE12_VOLUME_SWITCH_ENABLED;
    int iLine12Volume = DEFAULT_LINE12_VOLUME_VALUE;
    int bLine12SwitchLast = bLine12Switch;
    int iLine12VolumeLast = iLine12Volume;

    /* phones 3/4 output */
    int bPhones34Switch = DEFAULT_PHONES34_VOLUME_SWITCH_ENABLED;
    int iPhones34Volume = DEFAULT_PHONES34_VOLUME_VALUE;
    int bPhones34SwitchLast = bPhones34Switch;
    int iPhones34VolumeLast = iPhones34Volume;

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
    apply_state(midi, UDCE_LINE12, iLine12Volume, bLine12Switch);
    iLine12VolumeLast = iLine12Volume;
    bLine12SwitchLast = bLine12Switch;

    apply_state(midi, UDCE_PHONES34, iPhones34Volume, bPhones34Switch);
    iPhones34VolumeLast = iPhones34Volume;
    bPhones34SwitchLast = bPhones34Switch;

    log_info(
        "Monitoring ALSA user-defined control elements "
        "'%s', '%s', '%s', '%s' for changes...",
        UDCE_LINE12_VOLUME_NAME,
        UDCE_LINE12_SWITCH_NAME,
        UDCE_PHONES34_VOLUME_NAME,
        UDCE_PHONES34_SWITCH_NAME
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
            bEventFromLine12 = (
                BOOL_IS_TRUE(is_event_from_udce(event, pLine12VolumeID)) ||
                BOOL_IS_TRUE(is_event_from_udce(event, pLine12SwitchID))
            );
            bEventFromPhones34 = (
                BOOL_IS_TRUE(is_event_from_udce(event, pPhones34VolumeID)) ||
                BOOL_IS_TRUE(is_event_from_udce(event, pPhones34SwitchID))
            );
            if (BOOL_IS_FALSE(bEventFromLine12) && BOOL_IS_FALSE(bEventFromPhones34)) {
                /* evaluate next event */
                continue;
            }

            /* TODO refactor */
            /* Read state */
            if (BOOL_IS_TRUE(bEventFromLine12)) {
                iRet = read_alsa_udce_integer_value(ctl, pLine12VolumeID, &iLine12Volume);
                if (ERR_CODE_IS_ERROR(iRet)) {
                    log_error("Unable to read the value of the volume");
                    return iRet;
                }
                iRet = read_alsa_udce_boolean_value(ctl, pLine12SwitchID, &bLine12Switch);
                if (ERR_CODE_IS_ERROR(iRet)) {
                    log_error("Unable to read the value of the mute switch");
                    return iRet;
                }

                /* If the state changed, apply it */
                if ((iLine12Volume != iLine12VolumeLast) || (bLine12Switch != bLine12SwitchLast)) {
                    apply_state(midi, UDCE_LINE12, iLine12Volume, bLine12Switch);
                    iLine12VolumeLast = iLine12Volume;
                    bLine12SwitchLast = bLine12Switch;
                }
            }
            if (BOOL_IS_TRUE(bEventFromPhones34)) {
                iRet = read_alsa_udce_integer_value(ctl, pPhones34VolumeID, &iPhones34Volume);
                if (ERR_CODE_IS_ERROR(iRet)) {
                    log_error("Unable to read the value of the volume");
                    return iRet;
                }
                iRet = read_alsa_udce_boolean_value(ctl, pPhones34SwitchID, &bPhones34Switch);
                if (ERR_CODE_IS_ERROR(iRet)) {
                    log_error("Unable to read the value of the mute switch");
                    return iRet;
                }

                /* If the state changed, apply it */
                if ((iPhones34Volume != iPhones34VolumeLast) || (bPhones34Switch != bPhones34SwitchLast)) {
                    apply_state(midi, UDCE_PHONES34, iPhones34Volume, bPhones34Switch);
                    iPhones34VolumeLast = iPhones34Volume;
                    bPhones34SwitchLast = bPhones34Switch;
                }
            }

        } /* end of inner while (snd_clt_read(ctl, event) > 0) */

    } /* end of outer while (BOOL_IS_TRUE(gbServiceIsRunning)) */

    return ERR_OK;
}


/* *************************************************************************
 * CLI Helpers
 * ************************************************************************* */

/* Return true if the device ID in the configuration is supported
 * (or was not set), and false otherwise. */
static int validate_device_id(int bDeviceIDWasSet) {
    int iDeviceID = gpstConfiguration.iDeviceID;
    if (BOOL_IS_TRUE(
        ((BOOL_IS_FALSE(bDeviceIDWasSet)) && (iDeviceID == DEVICE_ID_NOT_SET)) ||
        (iDeviceID == RME_DEVICE_FAMILY_ADI2_DAC) ||
        (iDeviceID == RME_DEVICE_FAMILY_ADI2_PRO) ||
        (iDeviceID == RME_DEVICE_FAMILY_ADI24_PRO)
    )) {
        return BOOL_TRUE;
    }
    log_error("Invalid device ID: '0x%02X'", iDeviceID);
    return BOOL_FALSE;
}

/* Return true if the device name in the configuration is supported
 * (or it was not set), and false otherwise. */
static int validate_device_name() {
    const char *szDeviceName = gpstConfiguration.szDeviceName;
    size_t cbDeviceName = sizeof(gpstConfiguration.szDeviceName);
    if (BOOL_IS_TRUE(
        (strncmp(szDeviceName, DEVICE_NAME_AUTODETECT, cbDeviceName) == 0) ||
        (strncmp(szDeviceName, DEVICE_NAME_RME_ADI2_DAC, cbDeviceName) == 0) ||
        (strncmp(szDeviceName, DEVICE_NAME_RME_ADI2_PRO, cbDeviceName) == 0) ||
        (strncmp(szDeviceName, DEVICE_NAME_RME_ADI24_PRO, cbDeviceName) == 0)
    )) {
        return BOOL_TRUE;
    }
    log_error("Invalid device name: '%s'", gpstConfiguration.szDeviceName);
    return BOOL_FALSE;
}

/* Return true if the MIDI device in the configuration is supported
 * (or it was not set), and false otherwise. */
static int validate_midi_device() {
    /* NOTE: not checking if szMIDIDevice is in the form "hw:X[,Y[,Z]]"
     *       since the value might be provided by the user and
     *       it is not clear that that form is the only one accepted by ALSA. */
    const char *szMIDIDevice = gpstConfiguration.szMIDIDevice;
    size_t cbMIDIDevice = sizeof(gpstConfiguration.szMIDIDevice);
    if (BOOL_IS_TRUE(
        (strncmp(szMIDIDevice, MIDI_DEVICE_AUTODETECT, cbMIDIDevice) == 0) ||
        ((strnlen(szMIDIDevice, cbMIDIDevice) > 0))
    )) {
        return BOOL_TRUE;
    }
    log_error("Invalid midi device: '%s'", gpstConfiguration.szMIDIDevice);
    return BOOL_FALSE;
}

/* Return true if the line12/phones34 min/max DB values
 * in the configuration are supported (or they were not set),
 * and false otherwise. */
static int validate_min_max_db_values() {
    OptionMinMaxDB pstPairs[] = {
        {"--line12-min-db", gpstConfiguration.dLine12MinDB},
        {"--line12-max-db", gpstConfiguration.dLine12MaxDB},
        {"--phones34-min-db", gpstConfiguration.dPhones34MinDB},
        {"--phones34-max-db", gpstConfiguration.dPhones34MaxDB},
    };
    size_t cCount = sizeof(pstPairs) / sizeof(pstPairs[0]);

    for (size_t i = 0; i < cCount; i++) {
        if (BOOL_IS_TRUE(
            (pstPairs[i].dValueDB < RME_VALUE_VOLUME_MIN_DB) ||
            (pstPairs[i].dValueDB > RME_VALUE_VOLUME_MAX_DB)
        )) {
            log_error(
                "Invalid value '%0.1f' for option '%s': "
                "it must be between '%0.1f' dB and '%0.1f' dB",
                pstPairs[i].dValueDB,
                pstPairs[i].szOptionName,
                RME_VALUE_VOLUME_MIN_DB,
                RME_VALUE_VOLUME_MAX_DB
            );
            return BOOL_FALSE;
        }
    }

    for (size_t i = 0; i < cCount; i += 2) {
        if (BOOL_IS_TRUE(
            (pstPairs[i].dValueDB >= pstPairs[i + 1].dValueDB)
        )) {
            log_error(
                "The value of option '%s' must be smaller than "
                "the value of option '%s'. "
                "Got instead: '%0.1f' >= '%0.1f'.",
                pstPairs[i].szOptionName,
                pstPairs[i + 1].szOptionName,
                pstPairs[i].dValueDB,
                pstPairs[i + 1].dValueDB
            );
            return BOOL_FALSE;
        }
    }

    return BOOL_TRUE;
}

static void print_device_names() {
    static const char *szDeviceNames[] = {
        DEVICE_NAME_AUTODETECT,
        DEVICE_NAME_RME_ADI2_DAC,
        DEVICE_NAME_RME_ADI2_PRO,
        DEVICE_NAME_RME_ADI24_PRO
    };
    size_t cCount = sizeof(szDeviceNames) / sizeof(szDeviceNames[0]);

    printf("RME Product Family (Device Name)\n");
    for (size_t i = 0; i < cCount; i++) {
        printf("- '%s'\n", szDeviceNames[i]);
    }
}

static void print_device_ids() {
    DeviceIDWithName pszDeviceIDs[] = {
        {DEVICE_NAME_RME_ADI2_DAC, RME_DEVICE_FAMILY_ADI2_DAC},
        {DEVICE_NAME_RME_ADI2_PRO, RME_DEVICE_FAMILY_ADI2_PRO},
        {DEVICE_NAME_RME_ADI24_PRO, RME_DEVICE_FAMILY_ADI24_PRO}
    };
    size_t cCount = sizeof(pszDeviceIDs) / sizeof(pszDeviceIDs[0]);

    printf("ID\tRME Product Family\n");
    for (size_t i = 0; i < cCount; i++) {
        printf("0x%02X\t%s\n", pszDeviceIDs[i].chDeviceID, pszDeviceIDs[i].szDeviceName);
    }
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Creates ALSA mute and volume controls for RME ADI-2 devices.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -C, --card-number INTEGER   Use the ALSA card with the given index; if specified, overrides the card detected by --device-name; option --list-cards prints the detected ones\n");
    printf("  -d, --device-name STRING    RME device name (default: '%s'); option --list-device-names prints the supported ones\n", DEFAULT_DEVICE_NAME);
    printf("  -D, --device-id STRING      RME device ID; if specified, overrides the device ID detected by --device-name; option --list-device-ids prints the supported ones\n");
    printf("  -h, --help                  Show this help\n");
    printf("  -l, --line12-min-db DB      Minimum dB value for line output 1/2, corresponding to   0%% volume (default: %.1f dB)\n", gpstConfiguration.dLine12MinDB);
    printf("  -L, --line12-max-db DB      Maximum dB value for line output 1/2, corresponding to 100%% volume (default: %.1f dB)\n", gpstConfiguration.dLine12MaxDB);
    printf("      --list-cards            List all ALSA cards and exit\n");
    printf("      --list-device-ids       List all supported device IDs\n");
    printf("      --list-device-names     List all supported device names\n");
    printf("  -M, --midi-device STRING    Use the specified MIDI device: 'autodetect' or 'hw:X[,Y[,Z]]'\n");
    printf("      --no-uninstall-on-exit  If specified, do not uninstall ALSA user-defined control elements when the program exits\n");
    printf("  -p, --phones34-min-db DB    Minimum dB value for phones output 3/4, corresponding to   0%% volume (default: %.1f dB)\n", gpstConfiguration.dPhones34MinDB);
    printf("  -P, --phones34-max-db DB    Maximum dB value for phones output 3/4, corresponding to 100%% volume (default: %.1f dB)\n", gpstConfiguration.dPhones34MaxDB);
    printf("      --uninstall             Uninstall ALSA user-defined control elements and exit\n");
    printf("  -v, --verbose               Enable verbose output\n");
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
        {"line12-min-db",            required_argument, 0, 'l'},
        {"line12-max-db",            required_argument, 0, 'L'},
        {"list-cards",               no_argument,       0, 'X'},    /* the short form is not public */
        {"list-device-ids",          no_argument,       0, 'Y'},    /* the short form is not public */
        {"list-device-names",        no_argument,       0, 'Z'},    /* the short form is not public */
        {"midi-device",              required_argument, 0, 'M'},
        {"no-uninstall-on-exit",     no_argument,       0, 'U'},    /* the short form is not public */
        {"phones34-min-db",          required_argument, 0, 'p'},
        {"phones34-max-db",          required_argument, 0, 'P'},
        {"uninstall",                no_argument,       0, 'u'},    /* the short form is not public */
        {"verbose",                  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt = 0;
    int bDeviceIDWasSet = BOOL_FALSE;
    while ((opt = getopt_long(argc, argv, "C:d:D:hl:L:XYZM:Up:P:uv", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'C':
            gpstConfiguration.iCardNumber = atoi(optarg);
            break;
        case 'd':
            strncpy(gpstConfiguration.szDeviceName, optarg, DEVICE_NAME_MAX_LENGTH - 1);
            break;
        case 'D':
            gpstConfiguration.iDeviceID = (int)strtol(optarg, NULL, 0);
            bDeviceIDWasSet = BOOL_TRUE;
            break;
        case 'h':
            gpstConfiguration.bPrintUsageOnly = BOOL_TRUE;
            break;
        case 'l':
            gpstConfiguration.dLine12MinDB = atof(optarg);
            break;
        case 'L':
            gpstConfiguration.dLine12MaxDB = atof(optarg);
            break;
        case 'M':
            strncpy(gpstConfiguration.szMIDIDevice, optarg, MIDI_DEVICE_MAX_LENGTH - 1);
            break;
        case 'p':
            gpstConfiguration.dPhones34MinDB = atof(optarg);
            break;
        case 'P':
            gpstConfiguration.dPhones34MaxDB = atof(optarg);
            break;
        case 'u':
            gpstConfiguration.bUninstall = BOOL_TRUE;
            break;
        case 'U':
            gpstConfiguration.bUninstallOnExit = BOOL_FALSE;
            break;
        case 'v':
            gbLogVerbose = BOOL_TRUE;
            break;
        case 'X':
            gpstConfiguration.bListCards = BOOL_TRUE;
            break;
        case 'Y':
            gpstConfiguration.bListDeviceIDs = BOOL_TRUE;
            break;
        case 'Z':
            gpstConfiguration.bListDeviceNames = BOOL_TRUE;
            break;
        default:
            return EXIT_ERROR_USAGE_BAD_OPTION;
        }
    }

    if (BOOL_IS_FALSE(validate_device_name())) {
        return EXIT_ERROR_USAGE_BAD_VALUE;
    }

    if (BOOL_IS_FALSE(validate_device_id(bDeviceIDWasSet))) {
        return EXIT_ERROR_USAGE_BAD_VALUE;
    }

    if (BOOL_IS_FALSE(validate_midi_device())) {
        return EXIT_ERROR_USAGE_BAD_VALUE;
    }

    if (BOOL_IS_FALSE(validate_min_max_db_values())) {
        return EXIT_ERROR_USAGE_BAD_VALUE;
    }

    return ERR_OK;
}


/* *************************************************************************
 * CLI Main
 * ************************************************************************* */

int main(int argc, char *argv[]) {
    int iRet = ERR_OK;
    int iExitCode = EXIT_OK;
    snd_ctl_t *ctl = NULL;
    snd_rawmidi_t *midi = NULL;
    snd_ctl_elem_id_t *pLine12VolumeID = NULL;
    snd_ctl_elem_id_t *pLine12SwitchID = NULL;
    snd_ctl_elem_id_t *pPhones34VolumeID = NULL;
    snd_ctl_elem_id_t *pPhones34SwitchID = NULL;

    iRet = parse_args(argc, argv);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot parse command line arguments: exit error %d", iRet);
        print_usage(argv[0]);
        iExitCode = iRet;
        goto cleanup;
    }

    /* Print the usage and exit */
    if (BOOL_IS_TRUE(gpstConfiguration.bPrintUsageOnly)) {
        gbLogVerbose = BOOL_FALSE;
        print_usage(argv[0]);
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Print the list of cards and exit */
    if (BOOL_IS_TRUE(gpstConfiguration.bListCards)) {
        gbLogVerbose = BOOL_FALSE;
        print_cards();
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Print the list of device IDs and exit */
    if (BOOL_IS_TRUE(gpstConfiguration.bListDeviceIDs)) {
        gbLogVerbose = BOOL_FALSE;
        print_device_ids();
        iExitCode = EXIT_OK;
        goto cleanup;
    }

    /* Print the list of device names and exit */
    if (BOOL_IS_TRUE(gpstConfiguration.bListDeviceNames)) {
        gbLogVerbose = BOOL_FALSE;
        print_device_names();
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
    log_info("Using device ID '0x%02X'", gpstConfiguration.iDeviceID);
    log_info("Using ALSA card '%d'", gpstConfiguration.iCardNumber);

    /* Set the card hw string from the card number */
    set_card_hw_string();

    /* Open card */
    iRet = snd_ctl_open(&ctl, gpstConfiguration.szCardHw, 0);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot open ALSA card '%s': %s",
            gpstConfiguration.szCardHw,
            snd_strerror(iRet)
        );
        iExitCode = EXIT_ERROR_CANNOT_OPEN_CARD;
        goto cleanup;
    }
    log_verbose("Opened ALSA card '%s'", gpstConfiguration.szCardHw);

    if (BOOL_IS_TRUE(gpstConfiguration.bUninstall)) {
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
            gpstConfiguration.iCardNumber
        );
        iExitCode = EXIT_ERROR_CANNOT_FIND_MIDI_DEVICE;
        goto cleanup;
    }
    log_info("Using MIDI device '%s'", gpstConfiguration.szMIDIDevice);

    /* Open the MIDI device with flag 0 (default, blocking) */
    iRet = snd_rawmidi_open(NULL, &midi, gpstConfiguration.szMIDIDevice, 0);
    if (ERR_ALSA_CODE_IS_ERROR(iRet)) {
        log_error(
            "Cannot open MIDI device '%s': %s",
            gpstConfiguration.szMIDIDevice,
            snd_strerror(iRet)
        );
        iExitCode = EXIT_ERROR_CANNOT_OPEN_MIDI_DEVICE;
        goto cleanup;
    }

    /* Create user-defined control elements for line 1/2 output,
     * volume first, then switch */
    snd_ctl_elem_id_alloca(&pLine12VolumeID);
    iRet = install_control_element(ctl, pLine12VolumeID, UDCE_LINE12_VOLUME_NAME, BOOL_FALSE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add volume UDCE '%s'", UDCE_LINE12_VOLUME_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    snd_ctl_elem_id_alloca(&pLine12SwitchID);
    iRet = install_control_element(ctl, pLine12SwitchID, UDCE_LINE12_SWITCH_NAME, BOOL_TRUE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add switch UDCE '%s'", UDCE_LINE12_SWITCH_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    /* Create user-defined control elements for phones 3/4 output,
     * volume first, then switch */
    snd_ctl_elem_id_alloca(&pPhones34VolumeID);
    iRet = install_control_element(ctl, pPhones34VolumeID, UDCE_PHONES34_VOLUME_NAME, BOOL_FALSE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add volume UDCE '%s'", UDCE_PHONES34_VOLUME_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    snd_ctl_elem_id_alloca(&pPhones34SwitchID);
    iRet = install_control_element(ctl, pPhones34SwitchID, UDCE_PHONES34_SWITCH_NAME, BOOL_TRUE);
    if (ERR_CODE_IS_ERROR(iRet)) {
        log_error("Cannot add switch UDCE '%s'", UDCE_PHONES34_SWITCH_NAME);
        iExitCode = EXIT_ERROR_CANNOT_ADD_CONTROL;
        goto cleanup;
    }

    /* Run the monitoring loop */
    log_info(
        "Start control loop for device ID '0x%02X' "
        "on MIDI device '%s' of ALSA card number '%d'",
        gpstConfiguration.iDeviceID,
        gpstConfiguration.szMIDIDevice,
        gpstConfiguration.iCardNumber
    );
    iRet = monitor_loop(
        ctl,
        pLine12VolumeID,
        pLine12SwitchID,
        pPhones34VolumeID,
        pPhones34SwitchID,
        midi
    );
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
    if (BOOL_IS_TRUE(gpstConfiguration.bUninstall)) {
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
        if (BOOL_IS_TRUE(gpstConfiguration.bUninstallOnExit)) {
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
