import meshtastic
import meshtastic.serial_interface
from pubsub import pub
import time
import requests
from os import environ
from sys import argv, stderr

# -- CONFIGURATION OPTIONS --
# TODO: consider moving these to environment variables or args
#SERIAL_PATH = "/dev/cu.usbmodem94A99028C3401"
if len(argv) < 2:
    print("1 positional argument required.")
    raise SystemExit(1)

SERIAL_PATH = argv[1]

if "T1_PSK" not in environ:
    print("T1_PSK must be a defined environment variable.")
    raise SystemExit(1)

PSK = environ['T1_PSK']

if "T1_FRIDGEPATH" not in environ:
    print("T1_FRIDGEPATH must be a defined environment variable.")
    raise SystemExit(1)

FRIDGEPATH = environ['T1_FRIDGEPATH']

# ---------
# Load in fridge IDs
fridges = []
with open(FRIDGEPATH, "r") as fridgeFile:
    fridges = fridgeFile.readlines()

i = 0
while i < len(fridges):
    fridges[i] = fridges[i].strip()
    if(fridges[i] == "" or fridges[i].startswith("//")):
        fridges.pop(i)
        i -= 1
    i += 1

if len(fridges) < 1:
    print("No fridges defined in fridges.txt")
    raise SystemExit(1)

def report(fridge: str, temp, printRep=False):
    if printRep:
        print(f"REPORT: {fridge} @ {temp}")
    
    request = requests.post("https://t1.frykman.dev/api/report", headers={"X-Fridge": f"{fridge}", "X-PSK": PSK}, data=f"{temp}")

    if request.status_code == 403:
        print("Incorrect PSK defined. Report of temperature failed.")
    elif request.status_code != 200:
        print(f"Failed to report temperature (fridge: {fridge}, data: {temp}), received status code {request.status_code}.")

# ---------
# Meshtastic event handlers

needsReconnect = True

def onRecv(packet, interface):
    if "decoded" not in packet:
        return

    if "portnum" not in packet["decoded"]:
        return

    if packet["decoded"]["portnum"] != 'PRIVATE_APP':
        return

    payload: bytes = packet["decoded"]["payload"]
    
    if payload.startswith(b"TSNSR") == False:
        print("Not a temperature reading message.")
        return
    
    data = payload.split(b"_")
    if len(data) != 3:
        print("Invalid packet data.")
        return

    # data[0]: 'TSNSR'
    fridgeIDStr = data[1]
    tempStr = data[2]

    # convert values to integers
    try:
        global fridgeID
        global temp
        fridgeID = int(fridgeIDStr)
        temp = int(tempStr)
    except ValueError:
        print("Recieved non-integer fields. Not reporting.")
        return
    
    # Match fridge ID to fridge name
    if(fridgeID < 0 or fridgeID >= len(fridges)):
        print(f"Fridge ID of {fridgeID} is not in range [0, {len(fridges)})")
        return
    
    fridge = fridges[fridgeID]

    # -- Send data to the webserver --
    try:
        report(fridge, temp, printRep=True)
    except ConnectionError:
        stderr.writeln("Failed to report temperature, connection error.")
        stderr.flush()
        pass

def onDc(interface, topic=pub.AUTO_TOPIC):
    print("Connection lost! Will try to reconnect within the next 10 seconds.")
    global needsReconnect
    needsReconnect = True

def onCxn(interface, topic=pub.AUTO_TOPIC):
    print("Connection established.")
    global needsReconnect
    needsReconnect = False

pub.subscribe(onRecv, "meshtastic.receive.data")
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
