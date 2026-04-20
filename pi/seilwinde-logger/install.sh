#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="/opt/seilwinde-logger"
SERVICE_PATH="/etc/systemd/system/seilwinde-logger.service"
INSTALL_USER="${INSTALL_USER:-seilwinde}"
INSTALL_GROUP="${INSTALL_GROUP:-${INSTALL_USER}}"

sudo apt-get update
sudo apt-get install -y python3 python3-serial

sudo install -d -m 755 "${TARGET_DIR}"
sudo install -d -o "${INSTALL_USER}" -g "${INSTALL_GROUP}" -m 755 /var/log/seilwinde

sudo install -m 755 "${SCRIPT_DIR}/logger.py" "${TARGET_DIR}/logger.py"
sudo install -m 644 "${SCRIPT_DIR}/README.md" "${TARGET_DIR}/README.md"
sudo install -m 644 "${SCRIPT_DIR}/requirements.txt" "${TARGET_DIR}/requirements.txt"
sudo install -m 644 "${SCRIPT_DIR}/seilwinde-logger.service" "${SERVICE_PATH}"

sudo systemctl daemon-reload
sudo systemctl enable --now seilwinde-logger.service
