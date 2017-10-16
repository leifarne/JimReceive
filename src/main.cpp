// rf95_reliable_datagram_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple addressed, reliable messaging server
// with the RHReliableDatagram class, using the RH_RF95 driver to control a RF95 radio.
// It is designed to work with the other example rf95_reliable_datagram_client
// Tested with Anarduino MiniWirelessLoRa, Rocket Scream Mini Ultra Pro with the RFM95W


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//#include <FirebaseArduino.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#include <SPI.h>
#include <Wire.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WifiManager.h>         //https://github.com/tzapu/WiFiManager


#include "Adafruit_GFX.h"
#include <Adafruit_SSD1306.h>

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// #define FIREBASE_HOST "example.firebaseio.com"
// #define FIREBASE_AUTH "token_or_secret"


#define OLED_RESET 4
Adafruit_SSD1306 display(-1);

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

/* for feather32u4
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
*/

/* for feather m0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
*/

/* for ESP w/featherwing */
#define RFM95_CS  2    // "E"
#define RFM95_RST 16   // "D"
#define RFM95_INT 15   // "B"

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(rf95, SERVER_ADDRESS);

static bool lastHttpGetSuccessful = true;
static char GoogleSSLFingerprint[] = "‎71 65 fd 90 6f e3 b4 f0 80 72 2e 3d c1 2e 0a c7 fd 84 88 49";

// Set temperature boundaries
float max = 30;
float min = 20;


void initDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(2000);

  // text display tests
  /*
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Hello, world!");
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(3.141592);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print("0x"); display.println(0xDEADBEEF, HEX);
  display.display();
  delay(2000);
  */
  display.clearDisplay();
  display.display();
}



void initWifi() {
  WiFi.begin("Ikkefaen", NULL);
  Serial.print("Connecting to Ikkefaen.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


String createGASTemperatureUrl(String temp, String vbat, String rssi) {
  String s = "http://jimtemp-7abe4.appspot.com/log?";
  // String s = "https://script.google.com/macros/s/AKfycbyp3UIDM317eV6VhR-HWiBhIAsylpOoaD6C-TuX0SJNpMbZcQU/exec?";
  String parms = "appid=jim&chipid=6&bat=%b&temp=%t&db=%d";
  parms.replace("%t", temp);
  parms.replace("%b", vbat);
  parms.replace("%d", rssi);

  s.concat(parms);

  Serial.printf(s.c_str());
  Serial.println();

  return s;
}

String createTemperatureUrl(String temp, String vbat, String rssi) {
    String s = "http://dweet.io/dweet/for/jim-aberdeen-temperature?temperature=%f&battery=%b&signal=%s";
    s.replace("%f", temp);
    s.replace("%b", vbat);
    s.replace("%s", rssi);

    Serial.printf(s.c_str());
    Serial.println();

    return s;
}


void saveToCloud(float temp, float vbat, int rssi) {
    Serial.print("[HTTP] begin...\n");

    String url = createGASTemperatureUrl(String(temp), String(vbat), String(rssi));
    HTTPClient http;
    http.begin(url);

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode >= 200) {
            String payload = http.getString();
            Serial.println(payload);

            int comma = payload.indexOf(',', 1);
            String mins = payload.substring(1, comma);
            min = mins.toFloat();
            String maxs = payload.substring(comma+1, payload.indexOf(']', comma+1));
            max = maxs.toFloat();

            lastHttpGetSuccessful = true;
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %d\n", httpCode);
        lastHttpGetSuccessful = false;
    }

    http.end();
}


void setup()
{
  Serial.begin(9600);
  //while (!Serial) ; // Wait for serial port to be available
  Serial.println("initialising");

  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("SuperDuperAP");


  // Ensure serial flash is not interfering with radio communication on SPI bus
  //pinMode(LED_BUILTIN, OUTPUT);
  //digitalWrite(LED_BUILTIN, HIGH);

  //initWifi();
  initDisplay();

  if (!manager.init()) {
    Serial.println("init failed");
    while (true) ;
  }

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
//  driver.setTxPower(23, false);
  // If you are using Modtronix inAir4 or inAir9,or any other module which uses the
  // transmitter RFO pins and not the PA_BOOST pins
  // then you can configure the power transmitter power for -1 to 14 dBm and with useRFO true.
  // Failure to do that will result in extremely low transmit powers.
//  driver.setTxPower(14, true);
  // You can optionally require this module to wait until Channel Activity
  // Detection shows no activity on the channel before transmitting by setting
  // the CAD timeout to non-zero:
//  driver.setCADTimeout(10000);
}

uint8_t data[] = "And hello back to you";
// Dont put this on the stack:
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

int msgcount = 0;

void displayFloat(int textSize, int x, int y, float temp, const char *unit = NULL) {
  display.setTextSize(textSize);
  display.setCursor(x, y);
  display.print(temp, 1);
  if (unit) {
    display.print(unit);
  }
}

void displayInt(int textSize, int x, int y, int val, const char *unit = NULL) {
  display.setTextSize(textSize);
  display.setCursor(x, y);
  display.print(val);
  if (unit) {
    display.print(unit);
  }
}

void parseTempAndBat(const char* buf, float *temp, float *vbat) {
  String s = String((const char *) buf);

  int semicolon = s.indexOf(';');
  *temp = s.substring(0, semicolon).toFloat();
  *vbat = s.substring(semicolon+1).toFloat();
}


void displayScreen(float temp, float vbat, float min, float max, int rssi) {
  // Display on OLED
  display.clearDisplay();

  // Display temp in upper left, with big font
  // Invert text if outside bounds.
  if (min < temp && temp < max) {
    display.setTextColor(WHITE);
  }
  else {
    display.setTextColor(BLACK, WHITE);
  }
  displayFloat(4, 0, 0, temp);

  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(min, 1);
  display.print(" - ");
  display.print(max, 1);

  // Display bat and signal on righthand side. Tiny font.
  // Invert if battery is low
  if (vbat > 3.3) {
    display.setTextColor(WHITE);
  }
  else {
    display.setTextColor(BLACK, WHITE);
  }
  display.setTextSize(1);
  display.setCursor(100, 5);
  display.print(vbat, 1);
  display.print("v");

  // Signal strength
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(100, 20);
  display.print(rssi);

  // Wifi status
  display.setCursor(100, 40);
  if (lastHttpGetSuccessful) {
    display.print(")");
  }
  else {
    display.print("-");
  }
  if (WiFi.status() == WL_CONNECTED) {
    display.print(")");
  }
  else {
    display.print("-");
  }

  display.display();
}


void updateScreenTimer(unsigned long ms) {
  display.setTextSize(1);

  // Divide by 60 to update every 60 seconds, divide by 10 to update every 10 seconds.
  int minutes = ms / 1000 / 120;
  display.setTextColor(WHITE);

  display.fillRect(5, 40, 50, 8, BLACK); // Clear the bar.
  display.setCursor(5, 40);

  switch (minutes) {
  case 0:
    display.print(">----");
    break;
  case 1:
    display.print("->---");
    break;
  case 2:
    display.print("-->--");
    break;
  case 3:
    display.print("--->-");
    break;
  case 4:
    display.print("---->");
    break;
  default:
    display.setTextColor(BLACK, WHITE);
    display.print("-----");
    break;
  }
  display.display();
}

void logMessage(uint8_t from, int msgcount, char *buf, float temp, float vbat) {
  Serial.print("got request from : 0x");
  Serial.print(from, HEX);
  Serial.print(": (");
  Serial.print(msgcount);
  Serial.print(") ");
  Serial.println(buf);
  Serial.print(temp);
  Serial.print("/");
  Serial.println(vbat);
}

// TODO: Reload configure
// TODO: Connect to WIFI

float temp = 15;
float vbat = 1;
unsigned long int lastReceived = 0;


void loop()
{
  if (manager.available())
  {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
      lastReceived = millis();

      // Collect values to display
      parseTempAndBat((const char*) buf, &temp, &vbat);
      int rssi = rf95.lastRssi();
      int snr = rf95.lastSNR();

      logMessage(from, msgcount++, (char *) buf, temp, vbat);

      displayScreen(temp, vbat, min, max, rssi);
      saveToCloud(temp, vbat, rssi);
      //delay(2000);

      // Send a reply back to the originator client
      if (!manager.sendtoWait(data, sizeof(data), from))
        Serial.println("sendtoWait failed");
    }
  }
  else {
    updateScreenTimer(millis() - lastReceived);
  }
}
