#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#include <LiquidCrystal.h>
#include <SimpleDHT.h> 

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

// THIS SECTION WAS COPIED FROM DYLAN'S TEMP SKETCH

// configure the SimpleDHT (temp and humidity) sensor on Pin 7 (8 and 9 work too)
int pinDHT11 = 7;
SimpleDHT11 dht11(pinDHT11);

// configure parallel LCD to the "Hello World" LCD test guide to make sure the display works
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// (END SECTION)

#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "SPI"

// software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// prints a fatal error to the screen
void perr(const char* err) {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Error!");
  
  lcd.setCursor(0, 1);
  lcd.print(err);

  while(1);
}

int32_t idx;

void setup(void)
{
  // LCD init
  lcd.begin(16, 2);
  delay(2000); // give the sensor a moment to stabilize

  // BLE init
  if ( !ble.begin(VERBOSE_MODE) )
  {
    perr("Couldn't find Bluefruit");
  }


  ble.echo(false);
  ble.verbose(false);

  lcd.clear();
  lcd.setCursor(0, 0);

  // Displays the hardware address for the connected Bluefruit before doing anything.
  // Useful for figuring out what device this is in a scan when there's ~30 other LE devices around you.
  char addr[64];

  ble.atcommandStrReply("AT+BLEGETADDR", addr, 64, 200);

  lcd.print(addr);

  delay(5000);

  // Sets up the GATT temperature service
  ble.sendCommandCheckOK("AT+GATTCLEAR");
  ble.sendCommandCheckOK("AT+GATTADDSERVICE=UUID=0xD256");
  auto rep = ble.sendCommandWithIntReply("AT+GATTADDCHAR=UUID=0x0001,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=8,VALUE=55", &idx);
  lcd.setCursor(0, 1);
  lcd.print(rep); // Puts 1 on the second line (under the hardware address) if the service was set up correctly.
}
void loop(void)
{
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  
  // read the data from the sensor
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    delay(4000); // wait 4000ms before trying again
    return;
  }

  // The simpleDHT library returns celsius natively. have to convert to fahrenheit
  int fahrenheit = (temperature * 9 / 5) + 32;

  // clear the screen to print the new data
  //lcd.clear();
  
  // print Temperature to the top Row
  //lcd.setCursor(0, 0); 
  //lcd.print("Temp: ");
  //lcd.print(fahrenheit); // change this to 'temperature' if you prefer celsius
  //lcd.print((char)223);  // degree symbol
  //lcd.print("F");
  
  // print humidity to the bottom Row
  //lcd.setCursor(0, 1);
  //lcd.print("Humidity: ");
  //lcd.print(humidity);
  //lcd.print("%");
  
  // wait 4 seconds between reads, the cheap sensors need time because its a knockoff
  delay(4000);

  // END DYLAN'S SKETCH

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(fahrenheit);
  lcd.setCursor(0, 1);

  // Updates the GATT temperature characteristic with our new value
  // The mesh node will poll this number way less often than we're updating it.
  char buf[256];
  snprintf(buf, sizeof(buf), "AT+GATTCHAR=%ld,%d", idx, fahrenheit);

  lcd.print(buf);

  bool x = ble.sendCommandCheckOK(buf);
  if(!x) {
    delay(2000);
    perr("error");
  }
}
