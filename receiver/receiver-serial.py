import meshtastic
import meshtastic.serial_interface
from pubsub import pub
import time

# -- CONFIGURATION OPTIONS --
# TODO: consider moving these to environment variables or args
SERIAL_PATH = "/dev/cu.usbmodem94A99028C3401"

needsReconnect = True

def onRecv(packet, interface):
    if "decoded" not in packet:
        return

    if "portnum" not in packet["decoded"]:
        return
    
    if packet["decoded"]["portnum"] != 256:
        return
    
    print(f"Received packet with 256 port: ${packet}")
    # TODO: need to receive data
    # TODO: need to send the data to t1

def onDc(interface, topic=pub.AUTO_TOPIC):
    print("Connection lost!")
    global needsReconnect
    needsReconnect = True

def onCxn(interface, topic=pub.AUTO_TOPIC):
    print("Connection established.")
    global needsReconnect
    needsReconnect = False

def log(line, interface):
    print(f"log: {line}")

pub.subscribe(onRecv, "meshtastic.receive")
pub.subscribe(onDc, "meshtastic.connection.lost")
pub.subscribe(onCxn, "meshtastic.connection.established")
#pub.subscribe(log, "meshtastic.log.line")

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