#define DEBUG     1

#define DEVICE_NAME "BCG_SLAVE_"

#define NUM_PIXELS  60    // Number of NeoPixels in the strip
#define LED_PIN     13    // GPIO pin for NeoPixel strip
#define SWITCH_PIN  14    // GPIO pin for the switch input

#define DEBOUNCE_DELAY 500 // Debounce delay for switch input in milliseconds

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
uint32_t lastMillis = 0;

const String HELP = "Available commands:\n"
                    "SET_IP <ip_address> - Set the device IP address\n"
                    "SET_SUBNET <subnet_mask> - Set the subnet mask\n"
                    "SET_GATEWAY <gateway_ip> - Set the gateway IP address\n"
                    "SET_OUTIP <outgoing_ip> - Set the outgoing IP address\n"
                    "SET_INPORT <port_number> - Set the input port (default 7001)\n"
                    "SET_OUTPORT <port_number> - Set the output port (default 7000)\n"
                    "SET_ID <device_id> - Set the device ID (1-8)\n"
                    "GET - Get current configuration\n"
                    "IP - Show current IP address\n"
                    "MAC - Show current MAC address\n"
                    "HELP - Show this help message\n";

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

void getConfig() {
  SerialBT.printf("Device ID: %d\n",  device_id);
  SerialBT.printf("IP: %s\n",         ip.toString().c_str());
  SerialBT.printf("Subnet: %s\n",     subnet.toString().c_str());
  SerialBT.printf("Gateway: %s\n",    gateway.toString().c_str());
  SerialBT.printf("Out IP: %s\n",     outIp.toString().c_str());
  SerialBT.printf("In Port: %d\n",    inPort);
  SerialBT.printf("Out Port: %d\n",   outPort);
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

void processData(String data) {
  data.trim(); // Remove leading and trailing whitespace
  auto updateIP = [&](const String& prefix, IPAddress& target, int offset) {
    String value = data.substring(offset);
    if (target.fromString(value)) {
      saveNetworkConfig();
      SerialBT.printf("✅ %s updated and saved.\n", prefix.c_str());
    } else {
      SerialBT.printf("❌ Invalid %s format.\n", prefix.c_str());
    }
  };
  if (data.startsWith("SET_IP ")) { updateIP("IP", ip, 7); } 
  else if (data.startsWith("SET_SUBNET ")) { updateIP("Subnet", subnet, 11); } 
  else if (data.startsWith("SET_GATEWAY ")) { updateIP("Gateway", gateway, 12);  } 
  else if (data.startsWith("SET_OUTIP ")) { updateIP("OutIP", outIp, 10); } 
  else if (data.startsWith ("SET_INPORT ")) {
    int port = data.substring(10).toInt();
    if (port > 0 && port < 65536) { inPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ Input port set to %d and saved.\n", inPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }
  else if (data.startsWith("SET_OUTPORT ")) {
    int port = data.substring(12).toInt();
    if (port > 0 && port < 65536) { outPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ Output port set to %d and saved.\n", outPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }  
  else if (data.startsWith("SET_ID ")) {
    int id = data.substring(7).toInt();
    if (id >= 1 && id <= 8) {
      device_id = static_cast<uint8_t>(id);
      preferences.begin("CONFIG", false);
      preferences.putUInt("device_id", device_id);
      preferences.end();
      SerialBT.printf("✅ Device ID set to %d and saved.\n", device_id);
    } else {
      SerialBT.println("❌ Invalid Device ID. Must be between 1 and 8.");
    }
  }
  else if (data == "GET") { getConfig(); }
  else if (data == "IP") { SerialBT.printf("ETH IP: %s\n", ETH.localIP().toString().c_str());}
  else if (data == "MAC") { SerialBT.printf("ETH MAC: %s\n", ETH.macAddress().c_str());}
  else if (data.indexOf("HELP")>=0){
    SerialBT.println(HELP);
    Serial.println(HELP);
    return;
  } else if (data.indexOf("GET")>=0){
    getConfig();
    return;
  }
}

void readBTSerial(){
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    processData(incoming);
    if (DEBUG) {SerialBT.println(incoming);}
  }
}

void readSwitch(){
  if (millis() - lastMillis < DEBOUNCE_DELAY){ return; } // Debounce delay
  if (digitalRead(SWITCH_PIN) == LOW) { // Check if switch is pressed
    if (DEBUG) { Serial.println("Switch pressed"); }
    oscSend(device_id); 
    lastMillis = millis();
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
      Serial.println("ERROR: No Ethernet connection - Restarting ESP32... ");
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
  pinMode(SWITCH_PIN, INPUT_PULLUP); // Set switch pin as input with pull-up resistor
  loadConfig(); // Load device ID from Preferences
  SerialBT.begin(DEVICE_NAME + String(device_id)); // Initialize Bluetooth Serial
  stripInit();
  loadNetworkConfig(); // Load network configuration from Preferences
  ethInit(); // Initialize Ethernet
}

void loop() {
  readSwitch();
  readBTSerial();
}