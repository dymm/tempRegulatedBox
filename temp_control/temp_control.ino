/*
   Temperature probe DS18B20 connected to PIN 7.
   The LCD 1602A is connected following those instructions :
   LCD RS pin to digital pin 12
   LCD Enable pin to digital pin 11
   LCD D4 pin to digital pin 5
   LCD D5 pin to digital pin 4
   LCD D6 pin to digital pin 3
   LCD D7 pin to digital pin 2
   LCD R/W pin to ground
   LCD VSS pin to ground  
   LCD VCC pin to 5V
   220 resistor:
   ends to +5V and ground
   wiper to LCD VO pin (pin 3)
   It's driven by the library : http://www.arduino.cc/en/Tutorial/LiquidCrystal
*/

#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DS18B20.h>

#define ONE_WIRE_BUS 7

OneWire oneWire(ONE_WIRE_BUS);
DS18B20 sensor(&oneWire);

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  sensor.begin();
}

void loop() {
  sensor.requestTemperatures();
  while (!sensor.isConversionComplete());  // wait until sensor is ready
  
  lcd.clear();
  printTemperature(sensor.getTempC());
  
  delay(1000);
}

void printTemperature(float temperature) {
  char buf1[ 16 ];
  char buf2[ 16 ];
  dtostrf(temperature, 4, 2, buf1);
  sprintf( buf2, "%s C", buf1);
  lcd.setCursor(0, 0);
  lcd.print(buf2);
}
