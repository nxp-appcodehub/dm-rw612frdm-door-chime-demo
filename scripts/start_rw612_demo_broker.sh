#
# Copyright 2023 NXP
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Start the Wi-Fi hotspot
nmcli device wifi hotspot ssid piap password austin00

# Stop the Mosquitto default broker
sudo systemctl stop mosquitto

# Start the Mosquito broker with the custom config for this demo
mosquitto -c /home/piap/rw612-aircon-mqtt/mosquitto.conf -d

