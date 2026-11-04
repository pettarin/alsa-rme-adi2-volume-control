#!/bin/bash
#
# This file is part of the alsa-rme-adi2-volume-control project
#
# Usage: install.sh [options]
#
# Options:
#   --compile-from-source          Compile the service executable from source
#   --install-build-dependencies   Install build dependencies
#   --uninstall                    Remove installation
#
# Examples:
#   # remove a previous installation
#   bash install.sh --uninstall
#
#   # download the pre-built executable and install service
#   bash install.sh
#
#   # compile the executable from source and install service
#   bash install.sh --compile-from-source
#
#   # install the build dependencies, compile from source, and install service
#   bash install.sh --install-build-dependencies --compile-from-source
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
# (supported values: aarch64, x86_64)
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

# Absolute URL for the pre-built executable to download
SERVICE_EXECUTABLE_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$SERVICE_NAME/$ARCHITECTURE/$SERVICE_EXECUTABLE_FILE_NAME"

# Absolute destination paths
# (they do NOT depend on the structure of the source repository)
SERVICE_EXECUTABLE_DESTINATION_FILE_PATH="/usr/local/bin/$SERVICE_EXECUTABLE_FILE_NAME"
SERVICE_EXECUTABLE_DOWNLOADED_TMP_PATH="/tmp/$SERVICE_EXECUTABLE_FILE_NAME"
SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH="/lib/systemd/system/$SERVICE_SYSTEMD_UNIT_FILE_NAME"
SERVICE_UDEV_RULES_DESTINATION_FILE_PATH="/etc/udev/rules.d/$SERVICE_UDEV_RULES_FILE_NAME"

# Script CLI defaults
COMPILE_FROM_SOURCE=0
INSTALL_BUILD_DEPENDENCIES=0
INSTALL=1
UNINSTALL=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --compile-from-source)
            COMPILE_FROM_SOURCE=1
            shift
            ;;
        --install-build-dependencies)
            INSTALL_BUILD_DEPENDENCIES=1
            shift
            ;;
        --uninstall)
            INSTALL=0
            UNINSTALL=1
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --compile-from-source          Compile the service executable from source"
            echo "  --install-build-dependencies   Install build dependencies"
            echo "  --uninstall                    Remove installation"
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
    exit 0
fi

# Install build dependencies
if [[ $COMPILE_FROM_SOURCE -eq 1 ]]; then

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

    # x86_64 == PC, aarch64 == Raspberry Pi
    if [[ $ARCHITECTURE != "aarch64" ]] && [[ $ARCHITECTURE != "x86_64" ]]; then
        echo "ERROR: architecture has no pre-built executable."
        echo "Please try compiling from source instead (--compile-from-source)."
        exit 2
    fi

    if command -v curl &>/dev/null; then
        curl -s -o "$SERVICE_EXECUTABLE_DOWNLOADED_TMP_PATH" "$SERVICE_EXECUTABLE_SOURCE_URL"
    elif command -v wget &>/dev/null; then
        wget -q -O "$SERVICE_EXECUTABLE_DOWNLOADED_TMP_PATH" "$SERVICE_EXECUTABLE_SOURCE_URL"
    else
        echo "ERROR: unable to find either curl or wget to download a pre-built executable."
        echo "Please install either curl or wget and try again."
        exit 2
    fi
    echo "Downloading pre-built executable ... done"

    echo "Copying executable ..."
    cp "$SERVICE_EXECUTABLE_DOWNLOADED_TMP_PATH" "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH"
    chmod +x "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH"
    rm "$SERVICE_EXECUTABLE_DOWNLOADED_TMP_PATH"
    echo "Copying executable ... done"

fi

echo "Installing systemd service ..."
cp "$SERVICE_SYSTEMD_UNIT_SOURCE_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH"
systemctl daemon-reload
systemctl enable "$SERVICE_NAME"
systemctl restart "$SERVICE_NAME"
echo "Installing systemd service ... done"

echo "Installing udev rules ..."
cp "$SERVICE_UDEV_RULES_SOURCE_FILE_PATH" "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH"
udevadm control --reload-rules
udevadm trigger
echo "Installing udev rules ... done"

echo "Installation of $SERVICE_NAME complete"
exit 0

