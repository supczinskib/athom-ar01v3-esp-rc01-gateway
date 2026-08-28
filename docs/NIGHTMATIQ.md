# Steinel NightmatIQ Plus

The standard AR01V3 firmware includes optional support for an already
commissioned Steinel Bluetooth Mesh network. There is no separate firmware
image: the normal `ar01v3-01.yaml` through `ar01v3-10.yaml` entry points keep RF
433.92 MHz, IR, ESP-NOW, Flipper import, OTA, the web interface and Home
Assistant API.

## Bluetooth operating modes

AR01V3 starts one Bluetooth mode at a time:

- **Bluetooth Proxy** is the default when NightmatIQ is not configured or is
  disabled.
- **Bluetooth Mesh** is used when the NightmatIQ integration is enabled.

Changing the mode automatically reboots the gateway. Disabling NightmatIQ from
the web page preserves every imported key and address. Enabling it again does
not require another cloud login or backup download. A separate **Remove
configuration** action permanently erases the saved integration data.

## Requirements

- A NightmatIQ Plus visible in Steinel Connect and synchronized to a Steinel
  account.
- An AR01V3 within Bluetooth range of the device.
- A trusted LAN during first setup.
- The normal `esphome/secrets.yaml`; Steinel credentials and Mesh keys are not
  added to this file.

## Install and first setup

1. Build and install the normal receiver entry point, for example
   `esphome/ar01v3-03.yaml`.
2. Open the authenticated AR01V3 page, then select **Configuration Page** in
   the **Steinel NightmatIQ Plus** section, or open `http://DEVICE/steinel`
   directly.
3. Enter the Steinel account email and password and select **Download network
   list**.
4. Select the network containing NightmatIQ. Empty stale records are omitted.
5. Leave the node address empty unless the network has multiple similar nodes.
6. Leave **IV Index** at `0` to recover it from an authenticated network beacon.
   AR01V3 also reuses the last authenticated value cached for the selected
   Steinel network. Supplying a known current value can shorten first
   synchronization when no cached value exists.
7. Select **Install on AR01V3**. The gateway downloads the backup over HTTPS,
   validates it, saves only the required data and reboots in Mesh mode.

Large network backups are not retained in RAM. During import AR01V3 closes its
ESPHome API connections, releases Bluetooth, streams the backup into the
inactive OTA slot and reads only the fields required by the integration. This
does not change the partition table, the running image or NVS; the next OTA
update normally overwrites that temporary workspace. Home Assistant therefore
disconnects briefly during cloud setup and reconnects after the controlled
gateway restart. Expand **Diagnostics** at the bottom of the page to see the
last response size and the heap state before the cloud operation and after
Bluetooth is released.

The Steinel password is not written to flash or returned by the local API. The
browser does send it to AR01V3 over local HTTP, so perform first setup only on a
trusted LAN. Normal operation after import is local and does not require the
Steinel cloud.

## Automatic Mesh source-address recovery

The imported backup supplies the provisioner's allocated unicast range and the
addresses occupied by its nodes. AR01V3 derives an unused pool above the
highest occupied address, limited to at most 2048 addresses at the end of that
range. Its first source address is distributed through the pool using the Mesh
UUID, the ESP32 hardware MAC and a random installation identifier; it is not a
fixed address compiled into the firmware.

The pool, current address, network UUID, installation identifier and recovery
counters are stored in a separate versioned NVS record. Reimporting the same
network on the same gateway advances to another address instead of immediately
reusing the previous source with a reset sequence number. This prevents a
NightmatIQ Replay Protection List from silently rejecting otherwise valid
messages after configuration removal, reimport, or replacement of the gateway.

Automatic recovery is deliberately conservative. It starts only when the
NightmatIQ manufacturer report was captured during the current gateway boot,
Mesh has been ready for at least 60 seconds, the stack has accepted at least 10
transmissions, at least 10 requests have timed out and no Mesh response has
been received. It never runs during cloud setup, an active Access operation, a
Composition Data query or a pending restart. At most 16 automatic address
changes are allowed for one import. Any authenticated Mesh response marks the
current address as verified in NVS and permanently disables further automatic
changes for that imported configuration. A later restart while NightmatIQ is
temporarily powered off therefore does not consume another address.

## Disable without deleting data

Open `/steinel` and select **Disable NightmatIQ**. After reboot AR01V3 returns to
Bluetooth Proxy while retaining the network, keys, addresses and IV Index.
Select **Enable NightmatIQ** to restore Mesh mode from the same configuration.

Use **Remove configuration** only when changing networks or intentionally
erasing all saved Mesh material. Cleanup is performed in place in both
Bluetooth modes. A new network can be imported immediately after the page
reports that removal has completed; no manual gateway restart is required.

## Home Assistant

The ESPHome integration creates a separate `Steinel NightmatIQ Plus` device.
It contains `NightmatIQ Illuminance`, `NightmatIQ Twilight Threshold` (1–1500 lx),
`NightmatIQ Mode`, `NightmatIQ Mesh Ready` and `NightmatIQ Status`.
`NightmatIQ Refresh` requests an immediate new device read without changing its
configuration. `NightmatIQ Actual Light Output` is a usable binary sensor based
on authenticated live Mesh responses. It keeps the last confirmed ON/OFF state
across transient missed polls and becomes unavailable only after five minutes
without a valid response, or immediately when the integration is disabled or
removed.

Diagnostic entities expose `NightmatIQ Installed Firmware`, `NightmatIQ
Hardware Revision`, `NightmatIQ Manufacturer`, `NightmatIQ Company ID` and
`NightmatIQ Product ID`. `NightmatIQ Signal Strength` reports the RSSI in dBm
from the latest authenticated Mesh response and keeps that value until another
valid response arrives. The `/steinel` diagnostics also show the value and its
age, which distinguishes a weak current link from an old retained reading.
Hexadecimal IDs use lowercase digits; the registered Company ID `0x0563` is
shown as `0x0563 (Steinel GmbH)`. An optional Lovelace example is available in
`home-assistant/nightmatiq_dashboard_card.yaml`; replace its sample entity IDs
with those discovered for the gateway.

## Compatibility

The integration has been tested with the Steinel IS Digi NM 2E6915 NightmatIQ
Plus. Standard Bluetooth Mesh communication does not expose the running
bootloader version, so AR01V3 does not display it.
