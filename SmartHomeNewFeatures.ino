#include <NTPtimeESP.h>  //to use NTP
#include <ESP8266WiFi.h>  //for the esp :)
#include "FS.h"  //to use SPIFFS
#include "WebAuthentication.h"
#include "ssidAndPass.h"
//#include "pgmspace.h"
#define HTTPuser "123"
#define HTTPpass "123"
#define maxDevices 10
class Device {
  public:
    bool enable;
    int pin;
    String itsName[15];
};
//Device device[maxDevices];
void serveFileToClient(String fileFromHTTP, WiFiClient cl);
void XML_response(WiFiClient cl);
void SetLEDs(void);
String req;
uint8_t MAC_array[6];
char MAC_char[18];
const char* ssid = ssidFromInclude;
const char* password = passFromInclude;
boolean LED_state[4] = {0}; // stores the states of the LEDs
char LED1 = 16, LED2 = 5, LED3 = 4, LED4 = 2;
char SWITCH1  = 14, SWITCH2 = 12, SWITCH3 = 13;
// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
File webFile;
int val;
void setup()
{
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, 0);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, 0);
  pinMode(LED3, OUTPUT);
  digitalWrite(LED3, 0);
  pinMode(LED4, OUTPUT);
  digitalWrite(LED4, 0);
  pinMode(SWITCH1, INPUT);
  pinMode(SWITCH2, INPUT);
  pinMode(SWITCH3, INPUT);
  Serial.begin(115200);       // for debugging
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); //connect via WIFI
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  server.begin();           // start to listen for clients
  Serial.print("Server started at: ");
  Serial.println(WiFi.localIP());



  // initialize SD card
  Serial.println("Initializing SPIFFS filesystem...");
  if (!SPIFFS.begin()) {
    Serial.println("ERROR - SPIFFS initialization failed!");
    return;    // init failed
  }
  Serial.println("SUCCESS - SPIFFS initialized.");
  // check for index.htm file
  if (!SPIFFS.exists("/index.html")) {
    Serial.println("ERROR - Can't find index.html file!");
    return;  // can't find index file
  }
  Serial.println("SUCCESS - Found index.htm file.");
//  device[2].itsName[20] = "000000";
  //Serial.println(device[2].itsName[20]);
}

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  int  startedWaiting = millis();
  while (!client.available()) {//while no data in client's buffer: WAIT
    delay(1);
    if (millis() > startedWaiting + 5000) {
      client.stop();
      Serial.println("!!!Timed OUT!!!");
      return;
    }

  }

  Serial.println("Client sent something");
  String Header = "";
  req = "";
  bool lookingForFile = 0;
  // Read the first line of the request
  String firstLine = client.readStringUntil('\r');
  Serial.print("firstLine");
  Serial.println(firstLine);
  String requestedFile = firstLine.substring(4, firstLine.indexOf("HTTP") - 1);
  Serial.print("requestedFile=");
  Serial.println(requestedFile);
  if ((requestedFile.indexOf(".") != -1) && (requestedFile.indexOf("nocache") == -1)) {
    lookingForFile = 1;
    Serial.println("looking for file=1");

  } else {
    Serial.println("looking for file=0");
  }
  req += firstLine;
  req += client.readString();
  //req=client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  // Match the request
  int val;
  if (lookingForFile) {
    Serial.println(req.indexOf("HTTP"));
    //requestedFile = req.substring(4, req.indexOf("HTTP") - 1);//GET /XXXXXXX HTTP
    if (requestedFile == "/controlPanelNew.html") { //if file requested is controlPanel
      int authorization = req.indexOf("Authorization");
      Serial.println(authorization);
      String givenHash = req.substring(authorization + 21, authorization + 21 + 12);
      const char * c = givenHash.c_str(); //convert string to char
      Serial.println(givenHash);
      Serial.println(c);
      //if (req.indexOf("YWRtaW46YWFh")<0) {
      //req.substring(req.indexOf("Authorization"))
      // req.substring(authorization,)
      if (!checkBasicAuthentication(c, HTTPuser, HTTPpass)) { //if username&pass wasn't inserted or inserted wrong
        client.println("HTTP/1.1 401 Unauthorized");
        client.println("WWW-Authenticate: Basic realm=\"Secure\"");
        client.println("Content-Type: text/html");
        client.println();
        client.println("<html>Please contact the system administrator to get a username and password.</html>");
        client.stop();
        return;
      }
      else {
        Serial.print("authentication checked = ");
        Serial.println(checkBasicAuthentication(c, "admin", "aaa"));
      }
    }
    serveFileToClient(requestedFile, client);
  }
  else if (firstLine.indexOf("ajax_inputs") != -1) {
    Serial.println("ajax requested");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/xml");
    client.println("Connection: keep-alive");
    client.println();
    Serial.println("HTTP RESPONSE ENDED");
    SetLEDs();
    // send XML file containing input states
    XML_response(client);
    Serial.println("XML Sent");

  }
  else if (firstLine.indexOf("new_device") != -1) {
    Serial.println("new device sent");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.print("yo yo yo");
  }
  else if (requestedFile == "/") {
    //client.stop();
    // return;

    Serial.println("serving page...");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: keep-alive");
    client.println();
    // send web page
    webFile = SPIFFS.open("/index.html", "r");        // open web page file
    if (webFile) {
      while (webFile.available()) {
        client.write(webFile.read()); // send web page to client
      }
      webFile.close();
      Serial.println("Web Page Served!");

    }
    // Set GPIO2 according to the request
  }
  delay(1);      // give the web browser time to receive the data
  //client.stop(); // Commented out because the client is destroyed in the new LOOP();

}

void XML_response(WiFiClient cl)
{
  int count;                 // used by 'for' loops
  int sw_arr[] = {SWITCH1, SWITCH2, SWITCH3};  // pins interfaced to switches
  Serial.println("here");
  cl.print("<?xml version = \"1.0\"?>");
  Serial.println("there");
  cl.print("<inputs>");
  // read switches
  for (count = 0; count < 3; count++) {
    cl.print("<switch>");
    if (digitalRead(sw_arr[count])) {
      cl.print("ON");
    }
    else {
      cl.print("OFF");
    }

    cl.println("</switch>");
  }
  Serial.println("for loop ended");
  // checkbox LED states
  // LED1
  for (count = 0; count < 4; count++) {
    cl.print("<LED>");

    if (LED_state[count]) {

      cl.print("on");
    }
    else {
      cl.print("off");
    }
    cl.println("</LED>");
  }
  cl.print("</inputs>");

  Serial.println("XML function ended");

}
void SetLEDs(void)
{ Serial.println("leds started");

  // LED 1 (pin 6)
  if (req.indexOf("LED1=1") != -1) {
    LED_state[0] = 1;  // save LED state
    digitalWrite(LED1, HIGH);
  }
  else if (req.indexOf("LED1=0") != -1) {
    LED_state[0] = 0;  // save LED state
    digitalWrite(LED1, LOW);
  }
  Serial.println("led 1 ended");
  // LED 2 (pin 7)
  if (req.indexOf("LED2=1") != -1) {
    LED_state[1] = 1;  // save LED state
    digitalWrite(LED2, HIGH);
  }
  else if (req.indexOf("LED2=0") != -1) {
    LED_state[1] = 0;  // save LED state
    digitalWrite(LED2, LOW);
  } Serial.println("led 2 ended");
  // LED 3 (pin 8)
  if (req.indexOf("LED3=1") != -1) {
    LED_state[2] = 1;  // save LED state
    digitalWrite(LED3, HIGH);
  }
  else if (req.indexOf("LED3=0") != -1) {
    LED_state[2] = 0;  // save LED state
    digitalWrite(LED3, LOW);
  } Serial.println("led 3 ended");
  // LED 4 (pin 9)
  if (req.indexOf("LED4=1") != -1) {
    LED_state[3] = 1;  // save LED state
    digitalWrite(LED4, HIGH);
  }
  else if (req.indexOf("LED4=0") != -1) {
    LED_state[3] = 0;  // save LED state
    digitalWrite(LED4, LOW);
  }
  Serial.println("led 4 ended");
}

void serveFileToClient(String fileFromHTTP, WiFiClient cl) {
  Serial.println(fileFromHTTP);
  if (!SPIFFS.exists(fileFromHTTP)) {
    Serial.println("ERROR - Can't find " + fileFromHTTP);
    return;  // can't find index file
  }
  Serial.println("SUCCESS - Found " + fileFromHTTP);
  Serial.println("Serving file...");

  File webFile = SPIFFS.open(fileFromHTTP, "r");        // open web page file
  if (webFile) {
    cl.println("HTTP/1.1 200 OK");
    cl.println("Content-Type: text/html");
    cl.println("Connection: keep-alive");
    cl.println();
    while (webFile.available()) {
      cl.write(webFile.read()); // send web page to client
    }
    webFile.close();
    Serial.println("File SERVED successfully");
  }
  else {
    Serial.println("Can't find file");
  }


}
