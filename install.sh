#!/bin/bash
#
# This file is part of the alsa-rme-adi2-volume-control project
#
# Usage: bash install.sh [options]
#
# Options:
#   --from-source                   Install from source, instead of downloading a released package
#   --install-build-dependencies    Install build dependencies
#   --no-uninstall-first            Do not remove a previous installation before installing
#   --uninstall                     Remove a previous installation and exit without installing
#
# Examples:
#   # remove a previous installation and exit without installing
#   bash install.sh --uninstall
#
#   # install the service from a released package (including a pre-built executable)
#   bash install.sh
#
#   # as above, but skip uninstalling a previous installation (if it exists)
#   bash install.sh --no-uninstall-first
#
#   # install the service from source (from a git clone, recompiling the executable)
#   bash install.sh --from-source
#
#   # as above, but install the build dependencies first
#   bash install.sh --install-build-dependencies --from-source
#

set -euo pipefail

# Name of the service
SERVICE_NAME="alsa-rme-adi2-volume-control"

# Remote repository which the pre-built executable is downloaded from
REMOTE_REPOSITORY_URL_PREFIX="https://www.albertopettarin.it"

# ###########################################################################
# DO NOT MODIFY BELOW THIS LINE!
# ###########################################################################

# Architecture of the current machine,
# used to determine the pre-built executable to download
#
# Supported values:
# - amd64, x86_64                       -> amd64
# - aarch64, arm, armhf, armv7l, armv8l -> arm
ARCHITECTURE="$(arch)"

# Script and project directory absolute paths
SCRIPT_DIRECTORY_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIRECTORY_PATH="$SCRIPT_DIRECTORY_PATH"

# File names
SERVICE_SYSTEMD_UNIT_FILE_NAME="$SERVICE_NAME.service"
SERVICE_EXECUTABLE_FILE_NAME="$SERVICE_NAME"
SERVICE_UDEV_RULES_FILE_NAME="99-$SERVICE_NAME.rules"

# Absolute source paths
# (they depend on the structure of the source repository)
SERVICE_SYSTEMD_UNIT_SOURCE_FILE_PATH="$PROJECT_DIRECTORY_PATH/src/systemd/$SERVICE_SYSTEMD_UNIT_FILE_NAME"
SERVICE_UDEV_RULES_SOURCE_FILE_PATH="$PROJECT_DIRECTORY_PATH/src/udev/$SERVICE_UDEV_RULES_FILE_NAME"

# Absolute URLs of the files to be downloaded
SERVICE_EXECUTABLE_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$SERVICE_NAME/$ARCHITECTURE/$SERVICE_EXECUTABLE_FILE_NAME"
SERVICE_SYSTEMD_UNIT_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$SERVICE_NAME/repo/$SERVICE_NAME/src/systemd/$SERVICE_SYSTEMD_UNIT_FILE_NAME"
SERVICE_UDEV_RULES_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$SERVICE_NAME/repo/$SERVICE_NAME/src/udev/$SERVICE_UDEV_RULES_FILE_NAME"

# Absolute destination paths
# (they do NOT depend on the structure of the source repository)
SERVICE_EXECUTABLE_DESTINATION_FILE_PATH="/usr/local/bin/$SERVICE_EXECUTABLE_FILE_NAME"
SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH="/lib/systemd/system/$SERVICE_SYSTEMD_UNIT_FILE_NAME"
SERVICE_UDEV_RULES_DESTINATION_FILE_PATH="/etc/udev/rules.d/$SERVICE_UDEV_RULES_FILE_NAME"

# Keep track if either curl or wget is present
USE_CURL=0
USE_WGET=0

# Script CLI defaults
FROM_SOURCE=0
INSTALL_BUILD_DEPENDENCIES=0
UNINSTALL=1
UNINSTALL_ONLY=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --from-source)
            FROM_SOURCE=1
            shift
            ;;
        --install-build-dependencies)
            INSTALL_BUILD_DEPENDENCIES=1
            shift
            ;;
        --no-uninstall-first)
            UNINSTALL=0
            shift
            ;;
        --uninstall)
            UNINSTALL=1
            UNINSTALL_ONLY=1
            shift
            ;;
        -h|--help)
            echo "Usage: bash $0 [options]"
            echo ""
            echo "Options:"
            echo "  --from-source                   Install from source, instead of downloading a released package"
            echo "  --install-build-dependencies    Install build dependencies"
            echo "  --no-uninstall-first            Do not remove a previous installation before installing"
            echo "  --uninstall                     Remove a previous installation and exit"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check root
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root (sudo)"
    exit 1
fi

# Check that the architecture is supported, if not installing from source
if [[ $FROM_SOURCE -ne 1 ]]; then
  if [[ $ARCHITECTURE == "amd64" ]] || [[ $ARCHITECTURE == "x86_64" ]]; then
        ARCHITECTURE="amd64"
    elif [[ $ARCHITECTURE == "aarch64" ]] || [[ $ARCHITECTURE == "arm" ]] || [[ $ARCHITECTURE == "armhf" ]] || [[ $ARCHITECTURE == "armv7l" ]] || [[ $ARCHITECTURE == "armv8l" ]]; then
        ARCHITECTURE="arm"
    else
        echo "ERROR: architecture $ARCHITECTURE has no pre-built executable."
        echo "Please try compiling from source instead (--compile-from-source)."
        exit 2
    fi

    # update the URL accordingly
    SERVICE_EXECUTABLE_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$SERVICE_NAME/$ARCHITECTURE/$SERVICE_EXECUTABLE_FILE_NAME"
fi

# Check that either curl or wget is present, if not installing from source
if [[ $FROM_SOURCE -ne 1 ]]; then
    if command -v curl &>/dev/null; then
        USE_CURL=1
    elif command -v wget &>/dev/null; then
        USE_WGET=1
    else
        echo "ERROR: unable to find either curl or wget to download a pre-built executable."
        echo "Please install either curl or wget and try again."
        exit 2
    fi
fi

# Uninstall
if [[ $UNINSTALL -eq 1 ]]; then
    echo "Uninstalling service $SERVICE_NAME ..."

    # remove udev rules definition file
    rm -f "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH"
    udevadm control --reload-rules 2>/dev/null || true

    # stop, disable and remove systemd unit file
    systemctl stop "$SERVICE_NAME" 2>/dev/null || true
    systemctl disable "$SERVICE_NAME" 2>/dev/null || true
    rm -f "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH"
    systemctl daemon-reload

    # remove the executable
    rm -f "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH"

    echo "Uninstalling service $SERVICE_NAME ... done"

    if [[ $UNINSTALL_ONLY -eq 1 ]]; then
        echo "Uninstall only: exit without installing service $SERVICE_NAME"
        exit 0
    fi
fi

if [[ $FROM_SOURCE -eq 1 ]]; then

    # Install build dependencies
    if [[ $INSTALL_BUILD_DEPENDENCIES -eq 1 ]]; then
        echo "Installing build dependencies ..."
        if command -v apt &>/dev/null; then
            apt install -y -qq build-essential cmake libasound2-dev pkg-config
        elif command -v dnf &>/dev/null; then
            dnf install -y gcc cmake make alsa-lib-devel pkgconfig
        elif command -v pacman &>/dev/null; then
            pacman -S --noconfirm gcc cmake make alsa-lib pkgconf
        else
            echo "ERROR: package manager not found."
            echo "Refer to the documentation to learn how to install the build dependencies."
            exit 2
        fi
        echo "Installing build dependencies ... done"
    fi

    echo "Compiling executable from source ..."
    cd "$PROJECT_DIRECTORY_PATH/src/alsa"
    cmake .
    make
    echo "Compiling executable from source ... done"

    echo "Installing executable ..."
    make install
    echo "Installing executable ... done"

else

    echo "Downloading pre-built executable ..."
    if [[ $USE_CURL -eq 1 ]]; then
        curl -s -o "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH" "$SERVICE_EXECUTABLE_SOURCE_URL"
    elif [[ $USE_WGET -eq 1 ]]; then
        wget -q -O "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH" "$SERVICE_EXECUTABLE_SOURCE_URL"
    else
        # it should never happen...
        exit 2
    fi
    chmod +x "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH"
    echo "Downloading pre-built executable ... done"

fi

echo "Installing systemd service ..."
if [[ $FROM_SOURCE -eq 1 ]]; then
    cp "$SERVICE_SYSTEMD_UNIT_SOURCE_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH"
else
    if [[ $USE_CURL -eq 1 ]]; then
        curl -s -o "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_SOURCE_URL"
    elif [[ $USE_WGET -eq 1 ]]; then
        wget -q -O "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_SOURCE_URL"
    else
        # it should never happen...
        exit 2
    fi
fi
systemctl daemon-reload
systemctl enable "$SERVICE_NAME"
systemctl restart "$SERVICE_NAME"
echo "Installing systemd service ... done"

echo "Installing udev rules ..."
if [[ $FROM_SOURCE -eq 1 ]]; then
    cp "$SERVICE_UDEV_RULES_SOURCE_FILE_PATH" "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH"
else
    if [[ $USE_CURL -eq 1 ]]; then
        curl -s -o "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH" "$SERVICE_UDEV_RULES_SOURCE_URL"
    elif [[ $USE_WGET -eq 1 ]]; then
        wget -q -O "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH" "$SERVICE_UDEV_RULES_SOURCE_URL"
    else
        # it should never happen...
        exit 2
    fi
fi
udevadm control --reload-rules
udevadm trigger
echo "Installing udev rules ... done"

echo "Installation of $SERVICE_NAME complete"
exit 0

