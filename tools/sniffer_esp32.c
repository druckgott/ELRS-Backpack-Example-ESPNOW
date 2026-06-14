#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

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

void sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = pkt->payload;
    
    // Prüfe, ob es ein Action Frame ist (0xD0)
    if (payload[0] == 0xD0) {
        
        // Prüfe, ob die Ziel-MAC mit unserer Ziel-MAC übereinstimmt (Byte 4-9)
        bool match = true;
        for (int i = 0; i < 6; i++) {
            if (payload[4+i] != target_mac[i]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            Serial.print("[SNIFFER] ESP-NOW Frame entdeckt! Länge: ");
            Serial.println(pkt->rx_ctrl.sig_len);
            
            Serial.print("Ziel MAC: ");
            for(int i=0; i<6; i++) { Serial.printf("%02X:", payload[4+i]); }
            Serial.println();
            
            Serial.print("Absender MAC: ");
            for(int i=0; i<6; i++) { Serial.printf("%02X:", payload[10+i]); }
            Serial.println();
            
            Serial.print("RAW BYTES (Erste 40 Bytes): ");
            for(int i=0; i<40; i++) { Serial.printf("%02X ", payload[i]); }
            Serial.println("\n------------------------------------------------");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Starte WLAN Sniffer auf Kanal 1...");

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    
    // Aktiviere den Promiscuous Mode, um ALLE Pakete in der Luft zu lesen
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
}

void loop() {
    // Der Sniffer läuft im Hintergrund
    delay(1000);
}