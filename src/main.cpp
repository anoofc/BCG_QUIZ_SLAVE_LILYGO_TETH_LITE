#define DEBUG     1

#define DEVICE_NAME "BCG_SLAVE_"

#define NUM_PIXELS  60    // Number of NeoPixels in the strip
#define LED_PIN     13    // GPIO pin for NeoPixel strip

#include <Arduino.h>
#include "eth_properties.h"
#include <Adafruit_NeoPixel.h>
#include <OSCMessage.h>
#include <ETH.h>
#include <WiFiUdp.h>
#include <BluetoothSerial.h>
#include <Preferences.h>

BluetoothSerial SerialBT; // Bluetooth Serial
Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800); // NeoPixel strip on GPIO 13
Preferences preferences;  // Preferences for storing data
WiFiUDP Udp;

IPAddress ip, subnet, gateway, outIp;
uint16_t inPort = 7001;
uint16_t outPort = 7000;

uint8_t device_id;


void saveIPAddress(const char* keyPrefix, IPAddress address) {
  for (int i = 0; i < 4; i++) {
    String key = String(keyPrefix) + i;
    preferences.putUInt(key.c_str(), address[i]);
  }
}

IPAddress loadIPAddress(const char* keyPrefix, IPAddress defaultIP) {
  IPAddress result;
  for (int i = 0; i < 4; i++) {
    String key = String(keyPrefix) + i;
    result[i] = preferences.getUInt(key.c_str(), defaultIP[i]);
  }
  return result;
}

void saveNetworkConfig() {
  preferences.begin("CONFIG", false);
  saveIPAddress("ip", ip);
  saveIPAddress("sub", subnet);
  saveIPAddress("gw", gateway);
  saveIPAddress("out", outIp);
  preferences.putUInt("inPort", inPort); // Save input port
  preferences.putUInt("outPort", outPort); // Save output port
  preferences.end();
}

void loadConfig() {
  preferences.begin("CONFIG", true);
  device_id = preferences.getUInt("device_id", 0); // Load device ID
  if (device_id < 1 || device_id > 8) {
    device_id = 10; // Default to 1 if invalid
    preferences.putUInt("device_id", device_id); // Save default device ID
  }
  preferences.end();
  if (DEBUG) { Serial.println("Device ID: " + String(device_id)); }
}

void loadNetworkConfig() {
  preferences.begin("CONFIG", true);
  ip      = loadIPAddress("ip",  IPAddress(10, 255, 250, 150));
  subnet  = loadIPAddress("sub", IPAddress(255, 255, 254, 0));
  gateway = loadIPAddress("gw",  IPAddress(10, 255, 250, 1));
  outIp   = loadIPAddress("out", IPAddress(10, 255, 250, 129));
  inPort  = preferences.getUInt("inPort", 7001); // Load input port
  outPort = preferences.getUInt("outPort", 7000); // Load output port
  preferences.end();
}

void oscSend(int value) {
  char address[40];  // increase buffer size
  snprintf(address, sizeof(address), "/device/");
  if (DEBUG){ Serial.println(address); } // Debug: print the address to Serial
  OSCMessage msg(address);
  msg.add(value);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void readBTSerial(){
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    // processData(incoming);    
    if (DEBUG) {SerialBT.println(incoming);}
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH IP: ");
      Serial.println(ETH.localIP());
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      Serial.println("Restarting ESP32... ERROR: No Ethernet connection");
      ESP.restart(); // Restart ESP32 if disconnected
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      break;
    default:
      break;
  }
}

void ethInit() {
  ETH.begin( ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE_0);
  ETH.config(ip, gateway, subnet);
  WiFi.onEvent(WiFiEvent);
  Udp.begin(inPort);
  delay(10); // Wait for the Ethernet to initialize
  Serial.println("ETH Initialized");
  Serial.printf("ETH IP: %s\n", ETH.localIP().toString().c_str());
  Serial.printf("ETH MAC: %s\n", ETH.macAddress().c_str());
}

void stripInit() {
  strip.begin(); // Initialize the NeoPixel strip
  strip.setBrightness(255); // Set brightness to 50 (0-255)
  strip.show();
}


void setup() {
  Serial.begin(115200);
  loadConfig(); // Load device ID from Preferences
  SerialBT.begin(DEVICE_NAME + String(device_id)); // Initialize Bluetooth Serial
  stripInit();
  loadNetworkConfig(); // Load network configuration from Preferences
  ethInit(); // Initialize Ethernet
}

void loop() {
  
}