#include <ESP8266WiFi.h>        // Library for connecting to wifi
#include <WiFiClient.h>         // Library for connecting to wifi as a client
#include <ESP8266WiFiMulti.h>   // Library for both hosting and connecting to wifi
#include <ESP8266WebServer.h>   // Library for hosing a web server on an ESP8266
#include <WebSocketsServer.h>   // Library for Web Socket(connecting javascript to server)
#include <SPI.h>                // Library for using SPI features
#include <FS.h>                 // Library for storing files in SPIFFS
#include <ArduinoJson.h>        // Library for json parsing
#include <ESP8266Ping.h>        // Library for sending/recieving pings from ESP8266

char serverName[] = "mail.smtp2go.com";   // The SMTP Server


ESP8266WiFiMulti wifiMulti;         // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);        // Create a webserver object that listens for HTTP request on port 80
WebSocketsServer webSocket(81);     // create a websocket server on port 81
WiFiClient espClient;               // Create a client object
IPAddress ip(192,168,43,179);       // Formatting the IP of the client board

bool softwareButtonPressed = false;
bool firePing = false;
double minTemp = 0;
double maxTemp = 26;
bool sentBelowText = false;
bool sentAboveText = false;
char* phoneNumber = "8478269269";
char* phoneEnding = "@txt.att.net";
void getTempFromClient();
void handleNotFound();


String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)

/*
 * Setup handles with setting up the wifi and some hardware timers.
 * Most of the features like websockets, servers, and connections 
 * need this working wifi before initializing them, so all is done
 * before the board starts its loop.
*/
void setup(void) {
  //Setup the timer for pinging, to check if the client is still connected.
  timer1_attachInterrupt(checkIfOnISR);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_SINGLE);
  timer1_write(1562500);
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  wifiMulti.addAP("Samsung Galaxy S10+_7243", "tdem7012");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("Cat House", "Hammock2!");

  Serial.println("Connecting ...");
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());                                    // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());                                 // Send the IP address of the ESP8266 to the computer

  SPIFFS.begin();                                                 // Start the SPI Flash Files System
  startWebSocket();                                               // Start a WebSocket server
  ////////////////////////////////////////////////
  server.on("/softwareButton", HTTP_POST, getTempFromClient);     // Call the 'handleRoot' function when a client requests URI "/"

  server.onNotFound([]() {                                        // If the client requests any URI
    if (!handleFileRead(server.uri()))                            // send it if it exists
      server.send(404, "text/plain", "404: Not Found");           // otherwise, respond with a 404 (Not Found) error
  });
  server.begin();                                                 // Actually start the server
  Serial.println("HTTP server started");
  printCSV();
}

/*
 * Code for starting the websocket
 * as well as attaching event handlers to them.
 */
void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                                              // start the websocket server
  webSocket.onEvent(webSocketEvent);                              // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

/*
 * The main loop will constantly check information to and from
 * the clients and the websockets, as well as send pings if
 * the timer has run out(which means the client hasnt been communicating
 * with the server
*/
void loop(void) {
  webSocket.loop();
  server.handleClient();
  if (firePing)
  {
        Serial.println("Device Disconnected");
        String jsError = "Disconnected";
        webSocket.broadcastTXT(jsError);
        firePing = false;
      
  }
}

/*
 * Interrupt to show that its been too long, 
 * and that the client may be disconnected or
 * inactive.
*/
ICACHE_RAM_ATTR void checkIfOnISR(){
  firePing = true;
}


/*
 * The client ESP will send a json post with the temperature
 * once per second. The server reads this code, saves it in
 * a CSV file, and broadcasts it to the web for real time display.
*/
void getTempFromClient() {
    String payload = server.arg("plain");
    const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
    DynamicJsonBuffer jsonBuffer(capacity);
  
   // Parse JSON object
    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
      Serial.println(F("Parsing failed!"));
      server.send(503,"Network Timeout");
      return;
    }

    String newTemp;
    if (softwareButtonPressed == false)
    {
    newTemp = root["temp"].as<char*>();
    server.send(200,"Data Retrieved");
    Serial.print(F("Response:"));
    Serial.println(newTemp);
    }

    else
    {
     newTemp = root["temp"].as<char*>();
      Serial.print(F("Response:"));
      Serial.println(newTemp);
      server.send(303,"Turn On Display");
      softwareButtonPressed = false;
      Serial.println("Turning on Display..");
    }
 
      updateData(newTemp);

}

/*
 * Given a string value, it will call functions to update the CSV,
 * send data to the webpage, check if we need to send an email, and
 * do some debugging with incorrect values(i.e. an 85 shown at startup of
 * thermometer
*/
void updateData(String data){
  timer1_write(1562500);
if (data != "85")
{
  checkMinMax(data);
  writeValueToCSV(data);
  webSocket.broadcastTXT(data);
}
}

/*
 * If the client requests a link that is not stored on the server
*/
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

/*
 * Automatically return the file type of common files
 * found on the server, like js. Also handles gzip files
 * for compression.
*/
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

/*
 * Deals with sending a found file to the client. Will first
 * send a compressed file if it exists, otherwise it will
 * send an uncompressed file. Will also handle file not found errors
*/
bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed version
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

/*
 * Handles what to do for various types of websocket events. The connect and disconnect are more
 * for debugging, while the text is important for sending data in between the javascript
 * and the server.
 * Depending on the text code given, various events are called. Like updating the phone number,
 * min/max temperature, service provider, or turning on the LCD.\
*/
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      //Serial.printf("[%u] get Text: %s\n", num, payload);
      //Serial.println((char *)payload);
      char* softwarePush = "software";
      char* ATTPush = "AT&T";
      char* VerizonPush = "Verizon";
      char* SprintPush = "Sprint";
      char* minPush = "Min=";
      char* maxPush = "Max=";

      char* charpay = (char *)payload;

      
      if(strcmp(charpay, softwarePush) == 0)                  //Software Button Pressed
        {
        Serial.println("Software Button Pressed");
        if (softwareButtonPressed == false)
          {softwareButtonPressed = true;}
        }
      else if(strcmp(charpay, ATTPush) == 0)   
      {
        Serial.println("Carrier Set to AT&T");
        phoneEnding = "@txt.att.net";
      }
      else if(strcmp(charpay, VerizonPush) == 0)   
      {
        Serial.println("Carrier Set to Verizon");
        phoneEnding = "@vtext.com";
      }
      else if(strcmp(charpay, SprintPush) == 0)   
      {
        Serial.println("Carrier Set to Sprint");
        phoneEnding = "@messaging.sprintpcs.com";
      }
      else if(strncmp(charpay,minPush,4) == 0)
      {
        Serial.println("new Min");
        String n = strtok(charpay,"=");
        n = strtok(NULL," ");
        double newMinTemp = stringToDouble(n);
        minTemp = newMinTemp;
      }
      else if(strncmp(charpay,maxPush,4) == 0)
      {
        Serial.println("new Max");
        String n = strtok(charpay,"=");
        n = strtok(NULL," ");
        //Serial.println(n);
        double newMaxTemp = stringToDouble(n);
        maxTemp = newMaxTemp;
      }
      else
      {
       Serial.println("Phone Number set to ");
        Serial.print(charpay);
        phoneNumber = charpay;
      }
      
      
      break;

  }
}

/*
 * When a new temperature is seen, check to see if that is below the minimum
 * or above the maximum for sending notifications. A notification will not be 
 * sent more than once unless you go above the bound and then below it again.
*/
void checkMinMax(String data){
  double temp = stringToDouble(data);
  if (sentBelowText == true)
  {
    if (temp > minTemp)
    {
      sentBelowText = false;
    }
  }

  if (sentAboveText == true)
  {
    if (temp < maxTemp)
    {
      sentAboveText = false;
    }
  }


if (sentBelowText == false)
{
 if (temp < minTemp)
 {
  Serial.println("Sending Min Email");
  String rcpHolder = "";
  String phoneBeginningRCPT = "RCPT To: ";
  rcpHolder.concat(phoneBeginningRCPT);
  rcpHolder.concat(phoneNumber);
  rcpHolder.concat(phoneEnding);


  String toHolder;
  String phoneBeginningTo = "To: ";
  toHolder.concat(phoneBeginningTo);
  toHolder.concat(phoneNumber);
  toHolder.concat(phoneEnding);

   byte ret = sendEmail(rcpHolder,toHolder);
  sentBelowText = true;
 }
}

if (sentAboveText == false)
{
 if (temp > maxTemp)
 {
  Serial.println("Sending Max Email");
  String rcpHolder = "";
  String phoneBeginningRCPT = "RCPT To: ";
  rcpHolder.concat(phoneBeginningRCPT);
  rcpHolder.concat(phoneNumber);
  rcpHolder.concat(phoneEnding);


  String toHolder;
  String phoneBeginningTo = "To: ";
  toHolder.concat(phoneBeginningTo);
  toHolder.concat(phoneNumber);
  toHolder.concat(phoneEnding);

  byte ret = sendEmail(rcpHolder,toHolder);
  sentAboveText = true;
 }
}
}


/*
 * Code for sending emails to a phone. Much of it has to do with communicating with the 
 * STMP2GO server, which handles most of the work. We feed it an email address, and the message
 * along with the credentials for our STMP2GO account(encrypted) to be able to send.
 * The company was nice enough to unlock my account for student purposes.
*/
byte sendEmail(String rcptChar, String toChar)

{
  if (espClient.connect(serverName, 2525) == 1)
  {
    Serial.println(F("connected"));
  }
  else
  {
    Serial.println(F("connection failed"));
    return 0;
  }
  if (!emailResp())
    return 0;
  Serial.println(F("Sending EHLO"));
  espClient.println("EHLO www.example.com");
  if (!emailResp())
    return 0;
  Serial.println(F("Sending auth login"));
  espClient.println("AUTH LOGIN");
  if (!emailResp())
    return 0;
  Serial.println(F("Sending User"));
  espClient.println("ZXNwODI2NnVzZXI="); // Your encoded Username
  if (!emailResp())
    return 0;
  Serial.println(F("Sending Password"));
  espClient.println("ZXNwODI2NnBhc3M=");// Your encoded Password
  if (!emailResp())
    return 0;
  Serial.println(F("Sending From"));
  espClient.println(F("MAIL From: test@gmail.com")); // Enter Sender Mail Id
  if (!emailResp())
    return 0;
  Serial.println(F("Sending To"));
  espClient.println(rcptChar); // Enter Receiver Mail Id
  if (!emailResp())
    return 0;
  Serial.println(F("Sending DATA"));
  espClient.println(F("DATA"));
  if (!emailResp())
    return 0;
  Serial.println(F("Sending email"));
  espClient.println(toChar); // Enter Receiver Mail Id
  // change to your address
  espClient.println(F("From: ESP@fakedomain")); // Enter Sender Mail Id
  espClient.println(F("Subject: ESP8266 test e-mail\r\n"));
  espClient.println(F("Your Sensor is outside the bounds.\n"));
  espClient.println(F("Please check the sensor."));
  espClient.println(F("--Sent From ESP8266--."));
  //
  espClient.println(F("."));
  if (!emailResp())
    return 0;
  //
  Serial.println(F("Sending QUIT"));
  espClient.println(F("QUIT"));
  if (!emailResp())
    return 0;
  //
  espClient.stop();
  Serial.println(F("disconnected"));
  return 1;

}


 
/*
 * Error checking and seeing if the STMP server is having issues 
 * connecting to the host or sending to the client.
 */
byte emailResp()
{
  byte responseCode;
  byte readByte;
  int loopCount = 0;

  while (!espClient.available())
  {
    delay(1);
    loopCount++;
    if (loopCount > 20000)
    {
      espClient.stop();
      Serial.println(F("\r\nTimeout"));
      return 0;
    }
  }
  responseCode = espClient.peek();
  while (espClient.available())
  {
    readByte = espClient.read();
    Serial.write(readByte);
  }
  if (responseCode >= '4')
  {
    return 0;
  }
  return 1;
}


/*
 * Writing the inputted temperature to the CSV file
 */
void writeValueToCSV(String temperature)
{
      File tempLog = SPIFFS.open("/temp_data.csv", "a"); // Write the time and the temperature to the csv file
      tempLog.print(temperature);
      tempLog.print(',');
      tempLog.close(); 
}

void printCSV(){
  File file2 = SPIFFS.open("/temp_data.csv", "r");
 
  if (!file2) {
      Serial.println("No temperature data found. Creating a new file..");
      return;
  }
  
  while (file2.available()) {
      Serial.write(file2.read());
  }
}

double stringToDouble(String & str)   
{
  return atof( str.c_str() );
}
///////////////
