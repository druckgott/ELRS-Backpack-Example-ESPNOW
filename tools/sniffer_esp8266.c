#include <ESP8266WiFi.h>

// Deine Anlage (Wir suchen nach Paketen, die an diese Adresse gehen)
#include <Arduino.h>
#include <WiFi.h> // (Oder ESP8266WiFi.h beim ESP8266)

// Config laden, falls vorhanden
#if __has_include("my_config.h")
#include "my_config.h"
#endif

#ifdef MY_CUSTOM_UID
uint8_t target_mac[6] = MY_CUSTOM_UID;
#else
uint8_t target_mac[6] = {106, 19, 19, 206, 193, 30};
#endif

void sniffer_callback(uint8_t *buf, uint16_t len) {
    // Auf dem ESP8266 beginnt das echte WLAN-Paket nach einem 12-Byte Hardware-Header.
    // Um ganz sicher zu gehen, scannen wir einfach den gesamten Puffer nach deiner Ziel-MAC!
    
    bool found = false;
    int offset = 0;
    
    for(int i = 0; i < len - 40; i++) {
        if(buf[i] == target_mac[0] && buf[i+1] == target_mac[1] && 
           buf[i+2] == target_mac[2] && buf[i+3] == target_mac[3] && 
           buf[i+4] == target_mac[4] && buf[i+5] == target_mac[5]) {
            
            // MAC gefunden! Wir gehen 4 Bytes zurück, um den Start des Headers (D0 00 3C...) zu erwischen
            offset = i - 4; 
            if (offset >= 0 && buf[offset] == 0xD0) {
                found = true;
                break;
            }
        }
    }
    
    if (found) {
        Serial.print("[ESP8266 SNIFFER] LUFT-DATEN: ");
        for(int i = 0; i < 40; i++) {
            Serial.printf("%02X ", buf[offset + i]);
        }
        Serial.println();
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=============================================");
    Serial.println("ESP8266 SNIFFER BEREIT!");
    Serial.println("Warte auf Pakete vom ESP32 auf Kanal 1...");
    Serial.println("=============================================\n");

    wifi_set_opmode(STATION_MODE);
    wifi_set_channel(1);
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(sniffer_callback);
    wifi_promiscuous_enable(1);
}

void loop() {
    // Nichts zu tun, der Sniffer läuft asynchron im Hintergrund
    delay(10);
}