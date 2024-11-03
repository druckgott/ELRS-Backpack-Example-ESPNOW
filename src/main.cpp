#include <ESP8266WiFi.h>
#include <espnow.h>
#include <terseCRSF.h>  // https://github.com/zs6buj/terseCRSF   use v 0.0.6 or later
#include "crsf.h"  // Einbinden der crsf.h

// Please use ExpressLRS Configurator Runtime Options to obtain your UID (unique MAC hashed 
// from binding_phrase) Insert the six numbers between the curly brackets below
//For ESP NOW / ELRS Backpack you need to enter the UID resulting from hashing your 
//secret binding phrase. This must be obtained by launching https://expresslrs.github.io/web-flasher/, 
//enter your binding phrase, then make a note of the UID. Enter the 6 numbers between the commas
// Config Elrs Binding
//uint8_t UID[6] = {0,0,0,0,0,0}; // this is my UID. You have to change it to your once, should look 
uint8_t UID[6] = {106,19,19,206,193,30}; // this is my UID. You have to change it to your once (UID Input: 12345678)

const char* ssid = "Backpack_ELRS_Crsf";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points

// Externe Deklaration des CRSF-Objekts
extern CRSF crsf;  // Instanziierung des CRSF-Objekts
    
// Callback when data is received
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  Serial.print("ESPNow: len:");
  Serial.print(len);
  Serial.print(", ");
  // Annahme: printBytes(&*data, len); gibt jedes Byte des Arrays `data` im Hex-Format aus
  for (int i = 0; i < len; i++) {
      Serial.print(incomingData[i], HEX);  // Ausgabe jedes Bytes als Hexadezimalzahl
      if (i < len - 1) {
          Serial.print(" ");  // Leerzeichen zwischen den Bytes
      }
  }
  Serial.println();  // Neue Zeile am Ende der Ausgabe
  // For CRSF protocol from here
  espnow_len = len;
  crsf_len = len - 8;
  espnow_received = true;
  memcpy(&crsf.crsf_buf, &*(incomingData+8), sizeof(crsf.crsf_buf));
}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  Serial.println("Start");
  // MAC address can only be set with unicast, so first byte must be even, not odd --> important for BACKPACK
  UID[0] = UID[0] & ~0x01;
  WiFi.mode(WIFI_STA);
  // Set device as a Wi-Fi Station
  //WiFi.mode(WIFI_AP_STA); // Ermöglicht AP und Station gleichzeitig
  WiFi.disconnect();
  // Set a custom MAC address for the device in station mode (important for BACKPACK compatibility)
  wifi_set_macaddr(STATION_IF, UID); //--> important for BACKPACK
  // Start the device in Access Point mode with the specified SSID and password
  WiFi.softAP(ssid, password);
  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Set ESP-NOW Role
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  // Register peer
  esp_now_add_peer(UID, ESP_NOW_ROLE_COMBO, 0, NULL, 0);
  // Register a callback function to handle received ESP-NOW data
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
  //only for crsf Output
  crsfReceive(); 
}
