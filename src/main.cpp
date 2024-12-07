#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <terseCRSF.h>  // https://github.com/zs6buj/terseCRSF use v 0.0.6 or later
#include "crsf.h"  // Include the crsf.h

// Please use ExpressLRS Configurator Runtime Options to obtain your UID (unique MAC hashed 
// from binding_phrase) Insert the six numbers between the curly brackets below
//For ESP NOW / ELRS Backpack you need to enter the UID resulting from hashing your 
//secret binding phrase. This must be obtained by launching https://expresslrs.github.io/web-flasher/, 
//enter your binding phrase, then make a note of the UID. Enter the 6 numbers between the commas
// Config Elrs Binding
//uint8_t UID[6] = {0,0,0,0,0,0}; // this is my UID. You have to change it to your once, should look 
uint8_t UID[6] = {106,19,19,206,193,30}; // this is my UID. You have to change it to your once (UID Input: 12345678)

const char* ssid = "Backpack_ELRS_Crsf";           // SSID of the Access Point
const char* password = "12345678";                  // Passwort des Access Points

// External declaration of the CRSF object
extern CRSF crsf;  // Instantiation of the CRSF object

// Callback when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  Serial.print("ESPNow: len: ");
  Serial.print(len);
  Serial.print(", ");
  for (int i = 0; i < len; i++) {
      Serial.print(incomingData[i], HEX);  // Print each byte as hexadecimal
      if (i < len - 1) {
          Serial.print(" ");  // Space between bytes
      }
  }
  Serial.println();  // New line at the end of the output

  // For CRSF protocol
  espnow_len = len;
  crsf_len = len - 8;
  espnow_received = true;
  memcpy(&crsf.crsf_buf, &*(incomingData+8), sizeof(crsf.crsf_buf));
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("Start");

  // MAC address adjustment (ensure the first byte is even for BACKPACK compatibility)
  UID[0] = UID[0] & ~0x01;

  // Set device to station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Set a custom MAC address
  esp_wifi_set_mac(WIFI_IF_STA, UID);

  // Start Access Point with the specified SSID and password
  WiFi.softAP(ssid, password);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register a callback function to handle received ESP-NOW data
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Only for CRSF output
  crsfReceive();
}
