#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>   // Include the WebServer library
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>               // include SPI library
#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <ArduinoJson.h>


// Nokia 5110 LCD module connections (CLK, DIN, D/C, CS, RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(D6, D1, D2);

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

const int oneWireBus = D4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

const int buttonPin = D3;
const int backlightPin = D0;
volatile int displayStatus = 0;
volatile int updateDisplay = 0;
volatile int requestTemp = 0;
volatile float temperatureC = 0;
volatile float temperatureF = 0;
volatile bool wifiFailed = false;



void setup(void) {


  /////THERMOSTAT SETUP//////////////
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.setResolution(12);
  sensors.requestTemperatures();

  timer1_attachInterrupt(updateTempISR);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_SINGLE);
  timer1_write(312500);


  /////////////DISPLAY SETUP//////////
  display.begin();
  display.setContrast(60);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.display();
  pinMode(buttonPin, INPUT);
  pinMode(backlightPin, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(buttonPin), displayButtonPushedISR, CHANGE);

  //////////////////////////////////
  //SERVER SETUP////////////////////
  //////////////////////////////////
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  Serial.println('\n');

  wifiMulti.addAP("Samsung Galaxy S10+_7243", "tdem7012");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("Cat House", "Hammock2!");
  //wifiMulti.addAP("UI-DeviceNet", "UI-DeviceNet");

  Serial.println("Connecting ...");
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    if(i == 20){
      wifiFailed  = true;
      break;
    }
    delay(500);
    i++;
    Serial.print('.');
  }

  if(wifiFailed){
    Serial.println("WiFi connection Failed");
  }
  else{
    Serial.println('\n');
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());              // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
  }

}


void loop(void) {

  if (requestTemp) {
    temperatureC = sensors.getTempCByIndex(0);
    sensors.requestTemperatures();
    requestTemp = 0;
    updateDisplay = 1;
    if(!wifiFailed){
      sendTemp();
    }
  }

  if (updateDisplay) {
    updateDisplay = 0;
    display.clearDisplay();

    if (displayStatus) {
      if (temperatureC == -127.00){
        display.println("Sensor\nDisconnected");
      }
      else{
        display.print(temperatureC);
        display.println(" C");
      }
      digitalWrite(backlightPin, HIGH);
    }
    else{
      digitalWrite(backlightPin, LOW);
    }
    display.display();
  }
  


}

/////TEMP UPDATE EVERY SECOND ISR///////////
ICACHE_RAM_ATTR void updateTempISR() {
  requestTemp = 1;
  timer1_write(312500);
}

/////Button Pushed for display ISR/////////
ICACHE_RAM_ATTR void displayButtonPushedISR() {
  updateDisplay = 1;
  if (digitalRead(buttonPin) == LOW) {
    displayStatus = 1;
  }
  else {
    displayStatus = 0;
  }
}

///Sends temp via json to server
void sendTemp(){
  
  HTTPClient http;
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();
  JSONencoder["temp"] = temperatureC;
  char JSONmessageBuffer[300];
  JSONencoder.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  http.begin("http://192.168.43.59/softwareButton");
  http.addHeader("Content-Type","application/json");

  int httpCode = http.POST(JSONmessageBuffer);
  //String payload = http.getString();
  Serial.println(httpCode);

  if (httpCode == 303){
    displayStatus = !displayStatus;
    updateDisplay = 1;
  }
  
}
