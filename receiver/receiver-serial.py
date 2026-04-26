import meshtastic
import meshtastic.serial_interface
from pubsub import pub
import time
import requests
from os import environ

# -- CONFIGURATION OPTIONS --
# TODO: consider moving these to environment variables or args
SERIAL_PATH = "/dev/cu.usbmodem94A99028C3401"
if "T1_PSK" not in environ:
    print("T1_PSK must be a defined environment variable.")
    raise SystemExit

PSK = environ['T1_PSK']

needsReconnect = True

def report(fridge: str, temp):
    request = requests.post("https://t1.frykman.dev/api/report", headers={"X-Fridge": f"{fridge}", "X-PSK": PSK}, data=f"{temp}")

    if request.status_code == 403:
        print("Incorrect PSK defined. Report of temperature failed.")
    elif request.status_code != 200:
        print(f"Failed to report temperature (fridge: {fridge}, data: {temp}), received status code {request.status_code}.")

def onRecv(packet, interface):
    if "decoded" not in packet:
        return

    if "portnum" not in packet["decoded"]:
        return
    
    if packet["decoded"]["portnum"] != 256:
        return
    
    print(f"Received decoded packet with 256 port: ${packet}")
    # TODO: need to receive data

    # -- Send data to the webserver --
    # TODO: replace this with the actual data received.
    report("Star Farm", 99)

def onDc(interface, topic=pub.AUTO_TOPIC):
    print("Connection lost! Will try to reconnect within the next 10 seconds.")
    global needsReconnect
    needsReconnect = True

def onCxn(interface, topic=pub.AUTO_TOPIC):
    print("Connection established.")
    global needsReconnect
    needsReconnect = False

pub.subscribe(onRecv, "meshtastic.receive")
pub.subscribe(onDc, "meshtastic.connection.lost")
pub.subscribe(onCxn, "meshtastic.connection.established")

# ---------
# Connect to our discovered node

while True:
    if needsReconnect == True:
        print(f"Trying to connect to {SERIAL_PATH}...")

        try:
            interface = meshtastic.serial_interface.SerialInterface(devPath=SERIAL_PATH)
        except:
            print("Failed.")
            pass

    time.sleep(10) # this is needed to keep the connection alive