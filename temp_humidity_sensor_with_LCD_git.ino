#include <SimpleDHT.h> // import libraries, make sure its the SimpleDHT b/c cheap sensor
#include <LiquidCrystal.h>

// configure the SimpleDHT (temp and humidity) sensor on Pin 7 (8 and 9 work too)
int pinDHT11 = 7;
SimpleDHT11 dht11(pinDHT11);

// configure parallel LCD to the "Hello World" LCD test guide to make sure the display works
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() { //boot up
  lcd.begin(16, 2); // start the LCD, set the dimesnions for the text blocks (16 width 2 height)
  lcd.print("Waking Sensor..."); // for fun
  delay(2000); // give the sensor a moment to stabilize
}

void loop() {
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
  lcd.clear();
  
  // print Temperature to the top Row
  lcd.setCursor(0, 0); 
  lcd.print("Temp: ");
  lcd.print(fahrenheit); // change this to 'temperature' if you prefer celsius
  lcd.print((char)223);  // degree symbol
  lcd.print("F");
  
  // print humidity to the bottom Row
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print("%");
  
  // wait 4 seconds between reads, the cheap sensors need time because its a knockoff
  delay(4000); 
}