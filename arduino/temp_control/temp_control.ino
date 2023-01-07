/*
   Temperature probe DS18B20 connected to PIN 7.
   The AM232X captor is connected to SDL and SDA pins.
   The RTC clock is connected to the A4 -> SDA and A5 -> SCL.
   ESP8266 ESP-01 Wifi module : PIN 9 (software Tx) with a voltage divider 220 and 390 ohm connected to the module Rx, and PIN 10 (software Rx) connected to the module Tx.
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

#include <LiquidCrystal.h>  //LiquidCrystall by Paulo Costa, Arduino, Adafuit

//Needed : Adafruit BusIO, AL2320 sensor library by Adafruit
#include <OneWire.h> //OneWire : https://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DS18B20.h>  //DS18B20_RT by Rob Tillaart : https://github.com/RobTillaart/DS18B20_RT

#include <AM232X.h> // AM232X by Rob Tillaart : https://github.com/RobTillaart/AM232X

#include <RTClib.h> //RTCLib by Adafruit : https://github.com/adafruit/RTClib

//#include <SoftwareSerial.h>

#define FAN_BUS 10
#define ONE_WIRE_BUS 7

#define WITH_DS18B20
#ifdef WITH_DS18B20
#define NO_VALUE -999.0
OneWire oneWire(ONE_WIRE_BUS);
DS18B20 sensor(&oneWire);
bool isExternalSensorInitialized;
uint8_t isExternalSensorMeasuring;
float previousTemperature;
#endif

#define WITH_AM232X
#ifdef WITH_AM232X
AM232X am2320;
#endif
RTC_DS1307 rtc;  // Set up real time clock
uint8_t isTimeSet;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

//#define WIFI_BAUDRATE 9600
//#define WIFI_BAUDRATE 57600
#define WIFI_BAUDRATE 115200
#ifdef SOFTWARE_SERIAL
SoftwareSerial wifiSerial(10, 9);      // RX, TX for ESP8266
#else
#define wifiSerial Serial
#endif
bool WIFI_DEBUG = true;   //show more logs
int WIFI_TIMEOUT = 1000; //communication timeout
bool wifiConfigured = false;
String moduleIp = "";

void setup() {
  pinMode(FAN_BUS, OUTPUT);
  digitalWrite(FAN_BUS, LOW);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Init");
    
#ifdef WITH_DS18B20
  initDS18B20();
#endif  
  
#ifdef WITH_AM232X
  while (!am2320.begin() )
  {
    lcd.setCursor(0, 0);
    lcd.print("0");
    delay(1000);
  }
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("1");
  am2320.wakeUp();
#endif
  
  rtc.begin();
  isTimeSet = rtc.isrunning();
  if (!isTimeSet) {
    //Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    lcd.print("rtc is not running........");
  }
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("2");

   initWifi();
}

void loop() {
  if(!wifiConfigured) {
    handleWifiConfiguration();
  } else {
    doMeasures(); 
  }
  delay(1000);
}

void printTime() {
  lcd.setCursor(9, 0);
  if(!isTimeSet) {
    lcd.print("Not set");
  } else {
    DateTime now = rtc.now();  // Get the RTC info
    char buffer[16];
    sprintf(buffer, "%02d:%02d", now.hour(), now.minute());
    lcd.print(buffer);
  }
}

#ifdef WITH_DS18B20
void initDS18B20() {
  isExternalSensorMeasuring = 0;
  previousTemperature = NO_VALUE;  
  isExternalSensorInitialized = sensor.begin();
  if(isExternalSensorInitialized) {
    sensor.setResolution(12);
    sensor.setConfig(DS18B20_CRC);  // or 1
  }    
}

float readAndPrintTemperature() {
  if(!isExternalSensorInitialized) {
    initDS18B20();
    lcd.setCursor(0, 0);
    lcd.print("HS");
    return -999.0;
  }
  if(isExternalSensorMeasuring == 0) {
    sensor.requestTemperatures();
  }
  isExternalSensorMeasuring++;

  float temperature;
  if(!sensor.isConversionComplete()) {
    if(isExternalSensorMeasuring > 10) {
      isExternalSensorMeasuring = 0;
      lcd.setCursor(0, 0);
      lcd.print("Frozen...");
    }
    temperature = previousTemperature;
  } else {
    isExternalSensorMeasuring = 0;
    previousTemperature = temperature = sensor.getTempC();
  }
  if(temperature == DEVICE_CRC_ERROR || temperature == NO_VALUE) {
    return;
  }

  char buf1[16];
  char buf2[16];
  dtostrf(temperature, 4, 2, buf1);
  sprintf(buf2, "%s C", buf1);
  lcd.setCursor(0, 0);
  lcd.print(buf2);
  return temperature;
}

void driveFan(float temperature) {
  if (temperature > 24.0) {
    digitalWrite(FAN_BUS, HIGH);
  } else {
    digitalWrite(FAN_BUS, LOW);
  }
}
#endif

#ifdef WITH_AM232X
void readAndPrintValueFromAm2320() {

  int status = am2320.read();
  if(status == AM232X_OK) {
    float h = am2320.getHumidity();
    float t = am2320.getTemperature();
    lcd.setCursor(0, 1);  // move the cursor to the second row
    lcd.print(t);         // print the temperature to the LCD
    lcd.print(" C");      // print the units to the LCD

    lcd.setCursor(9, 1);  // move the cursor to the second row, column 12
    lcd.print(h);          // print the humidity to the LCD
    lcd.print("%");        // print the units to the LCD
  } else {
    float h = am2320.getHumidity();
    float t = am2320.getTemperature();
    lcd.setCursor(0, 1);  // move the cursor to the second row
    lcd.print(t);         // print the temperature to the LCD
    lcd.print(" C");      // print the units to the LCD

    lcd.setCursor(9, 1);  // move the cursor to the second row, column 12
    lcd.print(h);          // print the humidity to the LCD
    lcd.print("%");        // print the units to the LCD
  }
  #if 0
  float t, h;
  if (am2320.readTemperatureAndHumidity(&t, &h)) {
    lcd.setCursor(0, 1);  // move the cursor to the second row
    lcd.print(t);         // print the temperature to the LCD
    lcd.print(" C");      // print the units to the LCD

    lcd.setCursor(10, 1);  // move the cursor to the second row, column 12
    lcd.print(h);          // print the humidity to the LCD
    lcd.print("%");        // print the units to the LCD
  }
  #endif
}
#endif

void doMeasures() {
  lcd.clear();
  
#ifdef WITH_DS18B20
  float temperature = readAndPrintTemperature();
  driveFan(temperature);
#endif
  printTime();
#ifdef WITH_AM232X
  readAndPrintValueFromAm2320();
#endif
}


String sendToWifi(const char* command, boolean debug){
  if(debug)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(command);
    lcd.setCursor(0, 1);
    lcd.print("?");
    lcd.setCursor(0, 1);
  }
  wifiSerial.print(command); // send the read character to the esp8266
  wifiSerial.print("\r\n");
}

char* readFromWifi(char* buffer, const int size, const long int timeout, bool debug) {
  long int maxTime = millis() + timeout;
  int index = 0;
  memset(buffer, 0, size);
  do {
    while(index < (size-1) && wifiSerial.available())
    {
      buffer[index++] = wifiSerial.read();
    }
  } while(index < (size-1) && millis()<maxTime);

  if(debug)
  {    
    lcd.print(index < 12 ? buffer : buffer + index - 12);
  }
  return buffer;
}

void sendData(const char* str) {
  char cmd[32] = {0};
  snprintf(cmd, 32, "AT+CIPSEND=0,%d", strlen(str));  
  sendToWifi(cmd, WIFI_DEBUG);
  delay(200);
  sendToWifi(str, WIFI_DEBUG);
}

void closeConnection() {
  delay(100);
  sendToWifi("AT+CIPCLOSE=5", WIFI_DEBUG);
}

void getIp() {
  sendToWifi("AT+CIFSR", WIFI_DEBUG); // get ip address
  char response[100] = {0};
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);  

  char* ip = strstr(response, "\"");
  if(ip != NULL ) {
    char* nextQuote = strstr(ip + 1, "\"");
    if(nextQuote != NULL) {
      nextQuote[0] = '\0';
    }
  }

  lcd.setCursor(0, 1);
  lcd.print("IP:");
  lcd.print(ip + 1);
}

void getStatus() {
  sendToWifi("AT+CIPSTATUS", WIFI_DEBUG);
  char response[100] = {0};
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);  
  lcd.setCursor(0, 1);
  lcd.print("Status:");
  lcd.print(response);
}

void initWifi() {
  wifiSerial.begin(WIFI_BAUDRATE);
  wifiSerial.setTimeout(WIFI_TIMEOUT);
  while (!wifiSerial) {
    lcd.print("#");
    delay(500); // wait for serial port to connect. Needed for Leonardo only
  }

char response[100] = {0};
#if 1
  sendToWifi("AT+RST", WIFI_DEBUG);
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG); 
  delay(2000);

  sendToWifi("AT+CWJAP=\"Freebox-32C639\",\"hcxwrq64fsqn59bnq6k96d\"", WIFI_DEBUG);
  readFromWifi(response, 100, 10000, WIFI_DEBUG);
  delay(2000);
  if(strstr(response, "ERROR")!=NULL){
    return;
  }

  sendToWifi("AT+CWMODE=1", WIFI_DEBUG); // configure as access point
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);
#else
  sendToWifi("AT+CWMODE=2", WIFI_DEBUG); // configure as access point
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);
#endif
  if(strstr(response, "ERROR")!=NULL){
    return;
  }
  getIp();
  
  sendToWifi("AT+CIPMUX=1", WIFI_DEBUG); // configure for multiple connections
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);
  delay(100);

  sendToWifi("AT+CIPSERVER=1,80", WIFI_DEBUG); // turn on server on port 80
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);
  delay(100);

  //delay(5000);
  getIp();
}

void parseAndExecuteConnection(const char* ssidAndPass, char* response, const int maxLength) {
  char cmd[100] = {0};
  snprintf(cmd, 100, "AT+CWJAP=%s", ssidAndPass);
  sendToWifi(cmd, WIFI_DEBUG);
  readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);
  sendData(response);
}

void executeWifiCommand() {
  if(wifiSerial.available() <= 0) {
    return;
  }
  char buffer[100] = {0};
  readFromWifi(buffer, 100, WIFI_TIMEOUT, WIFI_DEBUG);
  
  char response[100] = {0};
  char* value;
  if((value = strstr(buffer, "esp8266:")) != NULL) {
    sendToWifi(value + 8, WIFI_DEBUG);
    readFromWifi(response, 100, WIFI_TIMEOUT, WIFI_DEBUG);
    char* result = strstr(response, "OK");
    if(result != NULL) {
      sendData("\n");
      sendData(response);
    }
    else {
      sendData("\n-------\n");
      sendData(result);
      sendData("\n-------\n");
    }
  }
  else if((value = strstr(buffer, "CONN:")) != NULL)
  {
    parseAndExecuteConnection(value + 5, response, 100);
  }
  else
  {
    if(WIFI_DEBUG) {
      //lcd.setCursor(0, 1);
      lcd.print("Ukn cmd");
    }
  }
}

void handleWifiConfiguration() {
  executeWifiCommand();
}
