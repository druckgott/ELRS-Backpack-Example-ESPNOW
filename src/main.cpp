#if defined(ESP32)
#pragma message "ESP32 stuff happening!"
#else
#pragma message "ESP8266 stuff happening!"
#endif

// ===== Logging Level =====
//#define LOG_LEVEL LOG_LEVEL_INFO   // oder INFO
//#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "logging.h"

// ======================================================
// 1. Includes
// ======================================================

#include <terseCRSF.h>  // https://github.com/zs6buj/terseCRSF   use v 0.0.6 or later
#include "msp.h"   // MSP Paket Handling
#include "msptypes.h"
#include "fake_vrx_fake_trainer.h"
#include <math.h>
#include "application.h"
#include "types.h"

// ======================================================
// 2. Platform Selection (ESP32 / ESP8266)
// ======================================================

#if defined(ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#else
#include <ESP8266WiFi.h>
#include <espnow.h>
#endif

enum RadioMode
{
    MODE_RX_ONLY = 0,
    MODE_TX_ONLY = 1,
    MODE_BOTH    = 2
};

// ======================================================
// 3. Configuration (Defines, UID, WiFi, Logging)
// ======================================================

// Please use ExpressLRS Configurator Runtime Options to obtain your UID (unique MAC hashed 
// from binding_phrase) Insert the six numbers between the curly brackets below
//For ESP NOW / ELRS Backpack you need to enter the UID resulting from hashing your 
//secret binding phrase. This must be obtained by launching https://expresslrs.github.io/web-flasher/, 
//enter your binding phrase, then make a note of the UID. Enter the 6 numbers between the commas
// Config Elrs Binding
//uint8_t UID[6] = {0,0,0,0,0,0}; // this is my UID. You have to change it to your once, should look 
uint8_t UID[6] = {106,19,19,206,193,30};


// ======================================================
// RadioMode Configuration
// ======================================================
//
// MODE_RX_ONLY  : Only receive ESP-NOW data (telemetry).
//                 Sending (trainer/VTX channels) is disabled.
//
// MODE_TX_ONLY  : Only send ESP-NOW data (trainer/VTX channels).
//                 Receiving telemetry is disabled.
//
// MODE_BOTH     : Send and receive ESP-NOW data.
//                 Full bidirectional operation.
//
// You can change the default mode below.
// It can also be changed later via Serial commands.
// ======================================================
RadioMode radioMode = MODE_BOTH;   // Default startup mode

// Time window (in milliseconds) after boot during which
// the RadioMode can be changed via Serial commands.
// After this time, the mode becomes fixed.
const unsigned long CONFIG_WINDOW_MS = 5000;  // 5 seconds

// ===== AP Wifi Config =====
const char* ssid = "Backpack_ELRS_Crsf";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points

// ===== Config for Channel output =====
const uint8_t NUM_CHANNELS = 16;
#define CHANNEL_MIN 172
#define CHANNEL_MAX 1811
// ===== in the Example there is a wave for the output channels that you can see the channes are moving
#define WAVE_SPEED 0.05
float wavePhase = 0.0;
#define STEP_INTERVAL 100   // 100ms Offset for each channel when the wave starts to the channel bevor

// ======================================================
// 4. Global Objects
// ======================================================


unsigned long configWindowStart = 0;
uint16_t rampChannels[NUM_CHANNELS];
uint32_t lastStepTime = 0;
uint8_t activeChannel = 0;
bool rampUp = true;

volatile bool espnow_received = false;
volatile uint16_t espnow_len = 0;
uint8_t espnow_buffer[250];
volatile uint16_t crsf_len = 0;

#if defined(ESP32)
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#endif

// ======================================================
// Platform Specific
// ======================================================
#if defined(ESP32)
QueueHandle_t rxqueue;
#endif

// ======================================================
// Module Instances
// ======================================================
FakeVRXFakeTrainer vrxModule;
MSP recv_msp;
CRSF crsf;

// ===============================
// RC Channels
// ===============================
uint16_t rc_channels[NUM_CHANNELS] = {0};

// ===============================
// Telemetry Values
// ===============================
int16_t hud_bat1_volts = 0;
int16_t hud_bat1_amps  = 0;
uint16_t hud_bat1_mAh  = 0;

bool motArmed = false;

// ===============================
// GPS Struct
// ===============================

Location hom = {0,0,0,0,0};
Location cur = {0,0,0,0,0};

// ===============================
bool finalHomeStored = false;

// ======================================================
// ESP-NOW Receive Callback
// ======================================================
#if defined(ESP32)
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
#endif

    if (len <= 8 || incomingData == NULL) return;

    if (incomingData[8] == 0 && incomingData[9] == 0 && len > 10) {
        return; 
    }

    crsf_len = len - 8;

    if (crsf_len > sizeof(espnow_buffer)) {
        crsf_len = sizeof(espnow_buffer);
    }

    memcpy(espnow_buffer, incomingData + 8, crsf_len);

    espnow_received = true;
}

// ======================================================
// ESP-NOW Send Callback
// ======================================================

#if defined(ESP32)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Beim ESP32 ist status 0 (ESP_NOW_SEND_SUCCESS)
    bool success = (status == ESP_NOW_SEND_SUCCESS);
#else
void OnDataSent(uint8_t *mac_addr, uint8_t status) {
    // Beim ESP8266 ist status 0 meist Erfolg
    bool success = (status == 0);
#endif

    if (success) {
        LOG_DEBUG_INLINE("ESP-NOW Send OK          ");
    } else {
        LOG_ERROR_INLINE("ESP-NOW Send FAIL (Status: %d) t=%lu ms      ", status, millis());
    }
}

// ======================================================
// 7. Setup()
// ======================================================

void initSerial()
{
    Serial.begin(115200);
    LOG_INFO("Start");
}

// Globale Variable für die gefälschte Sende-Ziel-MAC auf dem ESP32
#if defined(ESP32)
uint8_t fake_target_mac[6];
#endif

void initWiFi()
{
    UID[0] &= ~0x01;   // unicast fix
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    #if defined(ESP32)
        // Deine echte Identität bleibt die UID! (Wichtig für den RX-Empfang)
        esp_wifi_set_mac(WIFI_IF_STA, UID);
        
        uint8_t ap_mac[6];
        memcpy(ap_mac, UID, 6);
        ap_mac[5] ^= 0x01;
        esp_wifi_set_mac(WIFI_IF_AP, ap_mac);
        
        esp_wifi_start(); 
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    #else
        wifi_set_macaddr(STATION_IF, UID);
        wifi_set_channel(1);
    #endif
}

void initESPNow()
{
    #if defined(ESP32)
        if (esp_now_init() != ESP_OK)
    #else
        if (esp_now_init() != 0)
    #endif
    {
        LOG_ERROR("ESP-NOW init failed");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    #if defined(ESP32)
        // DER FIX FÜR DEN ESP32:
        // Wir erstellen eine gefälschte Ziel-MAC, indem wir das letzte Byte leicht abändern.
        // Damit umgehen wir die Hardware-Sperre für "Senden an sich selbst".
        memcpy(fake_target_mac, UID, 6);
        fake_target_mac[5] ^= 0xFF; // Ändert z.B. 4D zu B2 -> Für den ESP32 ein fremdes Gerät!

        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, fake_target_mac, 6);
        peerInfo.channel = 1;
        peerInfo.ifidx = WIFI_IF_STA; 
        peerInfo.encrypt = false;
        
        esp_now_del_peer(fake_target_mac);
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            LOG_ERROR("Failed to add Fake-Peer");
        }

        // Wir biegen JEDEN Sendeaufruf der Library in dieser Datei automatisch 
        // auf den registrierten Fake-Peer um. 
        #define esp_now_send(peer, data, len) esp_now_send(fake_target_mac, data, len)

    #else
        // FÜR ESP8266: Bleibt komplett unverändert auf der Original-Logik
        esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
        esp_now_add_peer(UID, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    #endif
}

void initRamp()
{
    for(int i = 0; i < NUM_CHANNELS; i++)
        rampChannels[i] = CHANNEL_MIN;
}

void initESP32Queue()
{
#if defined(ESP32)
    rxqueue = xQueueCreate(20, sizeof(mspPacket_t));

    if (rxqueue == NULL)
    {
        LOG_ERROR("Queue creation failed");
    }
#endif
}

void initInfo()
{
    LOG_INFO("==================================================");
    LOG_INFO("Radio Mode Configuration");
    LOG_INFO("Viewpoint: ESP Module Role (not the RC transmitter)");
    LOG_INFO("--------------------------------------------------");
    LOG_INFO("RX_ONLY : ESP receives ESP-NOW data and decodes it.");
    LOG_INFO("          Sending is disabled.");
    LOG_INFO("TX_ONLY : ESP sends ESP-NOW data.");
    LOG_INFO("          Receiving/decoding is disabled.");
    LOG_INFO("BOTH    : Full bidirectional operation.");
    LOG_INFO("--------------------------------------------------");
    LOG_INFO("Config window active for %lu ms", CONFIG_WINDOW_MS);
    LOG_INFO("Change mode via Serial during this window:");
    LOG_INFO("  1 = RX_ONLY");
    LOG_INFO("  2 = TX_ONLY");
    LOG_INFO("  3 = BOTH");
    LOG_INFO("==================================================");

    configWindowStart = millis();
}

void setup() {
    initSerial();
    delay(3000);
    initWiFi();
    initESPNow();
    initESP32Queue();
    vrxModule.init(UID);
    initRamp();
    initInfo();
}

// ======================================================
// 8. Loop()
// ======================================================

void loop()
{

    // ======================================================
    // Serial Mode Switch (only during config window)
    // ======================================================
    if (millis() - configWindowStart < CONFIG_WINDOW_MS)
    {
        if (Serial.available())
        {
            char c = Serial.read();

            // Nur echte Ziffern akzeptieren
            if (c >= '1' && c <= '3')
            {
                radioMode = (RadioMode)(c - '1');

                LOG_INFO("RadioMode changed to %d", radioMode + 1);
            }
        }
    }

    // ======================================================
    // ESP-NOW Receive Handling (CRSF)
    // ======================================================
   if (radioMode != MODE_TX_ONLY)
    {
        if (espnow_received)
        {
            uint16_t len_to_process = 0;

            #if defined(ESP32)
            portENTER_CRITICAL(&mux);
            #else
            noInterrupts();
            #endif

            len_to_process = crsf_len;
            memcpy(crsf.crsf_buf, espnow_buffer, len_to_process);
            espnow_received = false;

            #if defined(ESP32)
            portEXIT_CRITICAL(&mux);
            #else
            interrupts();
            #endif

            processCRSFFrame(crsf.crsf_buf, len_to_process);
        }
    }

    // ======================================================
    // Regular Mode
    // ======================================================
    if (radioMode != MODE_RX_ONLY)
    {
        vrxModule.updateChannelRamp();   // ESP-NOW senden
    }
}