#
# Copyright 2023 NXP
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Aircon-mqtt publish test script
import paho.mqtt.client as mqtt
import threading

BROKER_URL = "10.42.0.1"
BROKER_PORT = 1883
USER_ID = ""
USER_PSW = ""

# List of touples: (topic,data)
commands_sequence = [
    ("rw612/doorbell/sound", "On")
]

cmd_idx = 0

def publish_next():
    global commands_sequence
    global cmd_idx
    print(f"Sending topic: \"{commands_sequence[cmd_idx][0]}\" with data: \"{commands_sequence[cmd_idx][1]}\"")
    client.publish(commands_sequence[cmd_idx][0], payload=commands_sequence[cmd_idx][1], qos=1, retain=False)
    cmd_idx += 1

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    threading.Timer(10, publish_next).start()

def on_publish(client, userdata, result):
    if(cmd_idx < len(commands_sequence)):
        threading.Timer(10, publish_next).start()

client = mqtt.Client()
client.on_connect = on_connect
client.on_publish = on_publish
client.username_pw_set(USER_ID, USER_PSW)
print("Connecting to broker: " + BROKER_URL + f", Port: {BROKER_PORT}")
client.connect(BROKER_URL, BROKER_PORT, 60)
client.loop_forever()
