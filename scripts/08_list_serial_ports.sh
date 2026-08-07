#!/usr/bin/env bash
set -euo pipefail
if [[ -d /dev/serial/by-id ]]; then
  echo 'Serial ports:'
  find /dev/serial/by-id -maxdepth 1 -type l -print -exec readlink -f {} \;
else
  echo 'No /dev/serial/by-id directory was found. Connect AR01V3 with a data-capable USB-C cable.'
fi
