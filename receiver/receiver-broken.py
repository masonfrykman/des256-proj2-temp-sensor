import meshtastic
import meshtastic.ble_interface
from pubsub import pub
import time

# -- CONFIGURATION OPTIONS --
# TODO: consider moving these to environment variables
DEVICE_NAME = "FRYK_c340"

def onRecv(packet, interface):
    print(f"Received: ${packet}")

def onDc(interface, topic=pub.AUTO_TOPIC):
    print("Connection lost!")

def onCxn(interface, topic=pub.AUTO_TOPIC):
    print("Connection established via pub!")

def log(line, interface):
    print(f"log: {line}")

pub.subscribe(onRecv, "meshtastic.receive.data")
pub.subscribe(onDc, "meshtastic.connection.lost")
pub.subscribe(onCxn, "meshtastic.connection.established")
pub.subscribe(log, "meshtastic.log.line")

# ---------
# Scan for our specified node
print("Scanning for BLE devices... (takes up to 10 seconds)")
ble_devs = meshtastic.ble_interface.BLEInterface.scan()

print("Found:")
print(ble_devs)

active_device = None

for ble in ble_devs:
    if ble.name == DEVICE_NAME:
        active_device = ble
        break

if active_device == None:
    raise RuntimeError("Failed to find device with configured name.")


print(f"Connecting to {active_device.name} with address {active_device.address}")
# ---------
# Connect to our discovered node
interface = meshtastic.ble_interface.BLEInterface(address=active_device.address)

interface.sendHeartbeat()
print("Connection established, downloading node db")

while True:
    # this is needed to keep the connection alive
    time.sleep(1000)

interface.close()