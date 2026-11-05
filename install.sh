#!/bin/bash
#
# This file is part of the alsa-rme-adi2-volume-control project
#
# To print the usage message, run: bash install.sh --help
#
set -euo pipefail

# Name of the service
SERVICE_NAME="alsa-rme-adi2-volume-control"

# Remote repository which the pre-built executable is downloaded from
REMOTE_REPOSITORY_URL_PREFIX="https://www.albertopettarin.it/$SERVICE_NAME"

# ###########################################################################
# DO NOT MODIFY BELOW THIS LINE!
# ###########################################################################

# Architecture of the current machine,
# used to determine the pre-built executable to download
#
# Supported values:
# - amd64, x86_64              -> x86_64
# - arm, armhf, armv7l, armv8l -> arm
# - aarch64, arm64             -> aarch64
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
SERVICE_EXECUTABLE_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$ARCHITECTURE/$SERVICE_EXECUTABLE_FILE_NAME"
SERVICE_SYSTEMD_UNIT_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/repo/$SERVICE_NAME/src/systemd/$SERVICE_SYSTEMD_UNIT_FILE_NAME"
SERVICE_UDEV_RULES_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/repo/$SERVICE_NAME/src/udev/$SERVICE_UDEV_RULES_FILE_NAME"

# Absolute destination paths
# (they do NOT depend on the structure of the source repository)
SERVICE_EXECUTABLE_DESTINATION_FILE_PATH="/usr/local/bin/$SERVICE_EXECUTABLE_FILE_NAME"
SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH="/lib/systemd/system/$SERVICE_SYSTEMD_UNIT_FILE_NAME"
SERVICE_UDEV_RULES_DESTINATION_FILE_PATH="/etc/udev/rules.d/$SERVICE_UDEV_RULES_FILE_NAME"

# Keep track if either curl or wget is present
USE_CURL=0
USE_WGET=0

# Keep track if a package manager is present
USE_APT=0
USE_DNF=0
USE_PACMAN=0

# Script CLI defaults
FROM_SOURCE=0
INSTALL_BUILD_DEPENDENCIES=0
INSTALL_RUN_DEPENDENCIES=1
INSTALL_SYSTEMD=1
INSTALL_UDEV=1
UNINSTALL=1
UNINSTALL_ONLY=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --architecture)
            ARCHITECTURE="$2"
            shift 2
            ;;
        --from-source)
            FROM_SOURCE=1
            shift
            ;;
        --install-build-dependencies)
            INSTALL_BUILD_DEPENDENCIES=1
            shift
            ;;
        --no-install-run-dependencies)
            INSTALL_RUN_DEPENDENCIES=0
            shift
            ;;
        --no-install-systemd)
            INSTALL_SYSTEMD=0
            shift
            ;;
        --no-install-udev)
            INSTALL_UDEV=0
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
            echo "  --architecture STRING           Replace the architecture detected by command 'arch' with the provided STRING"
            echo "  --from-source                   Install from source, instead of downloading a released package"
            echo "  --install-build-dependencies    Install build dependencies"
            echo "  --no-install-systemd            Do not install the systemd service"
            echo "  --no-install-udev               Do not install the udev rules definition file"
            echo "  --no-install-run-dependencies   Do not install run dependencies"
            echo "  --no-uninstall-first            Do not remove a previous installation before installing"
            echo "  --uninstall                     Remove a previous installation and exit"
            echo ""
            echo "Note: this script must be executed as root, for instance with sudo"
            exit 0
            ;;
        *)
            echo "[ERRO] Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check root
if [[ $EUID -ne 0 ]]; then
    echo "[ERRO] This script must be executed as root, for instance with sudo."
    exit 2
fi

# Check that the architecture is supported, if not installing from source
if [[ $FROM_SOURCE -ne 1 ]]; then
    if [[ $ARCHITECTURE == "amd64" ]] || [[ $ARCHITECTURE == "x86_64" ]]; then
        # 64-bit OS on Intel/AMD 64-bit CPU (PC)
        ARCHITECTURE="x86_64"
    elif [[ $ARCHITECTURE == "arm" ]] || [[ $ARCHITECTURE == "armhf" ]] || [[ $ARCHITECTURE == "armv7l" ]] || [[ $ARCHITECTURE == "armv8l" ]]; then
        # 32-bit OS, even on a 64-bit CPU (e.g., Raspberry Pi 3)
        ARCHITECTURE="arm"
    elif [[ $ARCHITECTURE == "aarch64" ]] || [[ $ARCHITECTURE == "arm64" ]]; then
        # 64-bit OS on ARM 64-bit CPU (e.g., Raspberry Pi 5)
        ARCHITECTURE="aarch64"
    else
        echo "[ERRO] Architecture $ARCHITECTURE has no pre-built executable."
        echo "[ERRO] Supported architectures are: 'arm', 'arm64', 'x86_64'."
        echo "[ERRO] Please try running this script again compiling from source with --compile-from-source or overriding the detected architecture with --architecture."
        exit 3
    fi

    # update the URL accordingly
    SERVICE_EXECUTABLE_SOURCE_URL="$REMOTE_REPOSITORY_URL_PREFIX/$ARCHITECTURE/$SERVICE_EXECUTABLE_FILE_NAME"
    echo "[INFO] Architecture is '$ARCHITECTURE'"
    echo "[INFO] Service executable will be downloaded from URL '$SERVICE_EXECUTABLE_SOURCE_URL'"
fi

# Check that either curl or wget is present, if not installing from source
if [[ $FROM_SOURCE -ne 1 ]]; then
    if command -v curl &>/dev/null; then
        USE_CURL=1
    elif command -v wget &>/dev/null; then
        USE_WGET=1
    else
        echo "[ERRO] Unable to find curl nor wget to download a released package."
        echo "[ERRO] Please install curl or wget, and try running this script again."
        exit 3
    fi
fi

# Check that a package manager is present, if installing dependencies
if [[ $INSTALL_BUILD_DEPENDENCIES -eq 1 ]] || [[ $INSTALL_RUN_DEPENDENCIES -eq 1 ]]; then
    if command -v apt &>/dev/null; then
        USE_APT=1
    elif command -v dnf &>/dev/null; then
        USE_DNF=1
    elif command -v pacman &>/dev/null; then
        USE_PACMAN=1
    else
        echo "[ERRO] Unable to find a package manager."
        echo "[ERRO] Refer to the documentation to learn how to install the build and/or run dependencies."
        exit 3
    fi
fi

# Uninstall
if [[ $UNINSTALL -eq 1 ]]; then
    echo "[INFO] Uninstalling service $SERVICE_NAME ..."

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

    echo "[INFO] Uninstalling service $SERVICE_NAME ... done"

    if [[ $UNINSTALL_ONLY -eq 1 ]]; then
        echo "[INFO] Option --uninstall specified: exit without installing service $SERVICE_NAME"
        exit 0
    fi
fi

# Install run dependencies
if [[ $INSTALL_RUN_DEPENDENCIES -eq 1 ]]; then
    echo "[INFO] Installing run dependencies ..."
    if [[ $USE_APT -eq 1 ]]; then
        apt install -y -qq libasound2-dev
    elif [[ $USE_DNF -eq 1 ]]; then
        dnf install -y alsa-lib-devel
    elif [[ $USE_PACMAN -eq 1 ]]; then
        pacman -S --noconfirm alsa-lib
    else
        # it should never happen...
        exit 3
    fi
    echo "[INFO] Installing run dependencies ... done"
fi

if [[ $FROM_SOURCE -eq 1 ]]; then

    # Install build dependencies
    if [[ $INSTALL_BUILD_DEPENDENCIES -eq 1 ]]; then
        echo "[INFO] Installing build dependencies ..."
        if [[ $USE_APT -eq 1 ]]; then
            apt install -y -qq build-essential cmake libasound2-dev pkg-config
        elif [[ $USE_DNF -eq 1 ]]; then
            dnf install -y gcc cmake make alsa-lib-devel pkgconfig
        elif [[ $USE_PACMAN -eq 1 ]]; then
            pacman -S --noconfirm gcc cmake make alsa-lib pkgconf
        else
            # it should never happen...
            exit 3
        fi
        echo "[INFO] Installing build dependencies ... done"
    fi

    echo "[INFO] Compiling service executable from source ..."
    cd "$PROJECT_DIRECTORY_PATH/src/alsa"
    cmake .
    make
    echo "[INFO] Compiling service executable from source ... done"

    echo "[INFO] Installing service executable ..."
    make install
    echo "[INFO] Installing service executable ... done"

else

    echo "[INFO] Downloading pre-built service executable ..."
    if [[ $USE_CURL -eq 1 ]]; then
        curl -s -o "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH" "$SERVICE_EXECUTABLE_SOURCE_URL"
    elif [[ $USE_WGET -eq 1 ]]; then
        wget -q -O "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH" "$SERVICE_EXECUTABLE_SOURCE_URL"
    else
        # it should never happen...
        exit 3
    fi
    chmod +x "$SERVICE_EXECUTABLE_DESTINATION_FILE_PATH"
    echo "[INFO] Downloading pre-built service executable ... done"

fi

if [[ $INSTALL_SYSTEMD -eq 1 ]]; then
    echo "[INFO] Installing systemd service ..."
    if [[ $FROM_SOURCE -eq 1 ]]; then
        cp "$SERVICE_SYSTEMD_UNIT_SOURCE_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH"
    else
        if [[ $USE_CURL -eq 1 ]]; then
            curl -s -o "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_SOURCE_URL"
        elif [[ $USE_WGET -eq 1 ]]; then
            wget -q -O "$SERVICE_SYSTEMD_UNIT_DESTINATION_FILE_PATH" "$SERVICE_SYSTEMD_UNIT_SOURCE_URL"
        else
            # it should never happen...
            exit 3
        fi
    fi
    systemctl daemon-reload
    systemctl enable "$SERVICE_NAME"
    systemctl restart "$SERVICE_NAME"
    echo "[INFO] Installing systemd service ... done"
fi

if [[ $INSTALL_UDEV -eq 1 ]]; then
echo "[INFO] Installing udev rules ..."
    if [[ $FROM_SOURCE -eq 1 ]]; then
        cp "$SERVICE_UDEV_RULES_SOURCE_FILE_PATH" "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH"
    else
        if [[ $USE_CURL -eq 1 ]]; then
            curl -s -o "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH" "$SERVICE_UDEV_RULES_SOURCE_URL"
        elif [[ $USE_WGET -eq 1 ]]; then
            wget -q -O "$SERVICE_UDEV_RULES_DESTINATION_FILE_PATH" "$SERVICE_UDEV_RULES_SOURCE_URL"
        else
            # it should never happen...
            exit 3
        fi
    fi
    udevadm control --reload-rules
    udevadm trigger
    echo "[INFO] Installing udev rules ... done"
fi

echo "[INFO] Installation of $SERVICE_NAME complete"
exit 0

