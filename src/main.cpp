#if defined(ESP32)
#pragma message "ESP32 stuff happening!"
#else
#pragma message "ESP8266 stuff happening!"
#endif

// ======================================================
// 1. Includes
// ======================================================

//#include <terseCRSF.h>  // https://github.com/zs6buj/terseCRSF   use v 0.0.6 or later
#include "crsf.h"  // Einbinden der crsf.h
#include "msp.h"   // MSP Paket Handling
#include "msptypes.h"
#include "fake_vrx_fake_trainer.h"
#include <math.h>

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
#define NUM_CHANNELS 16
#define CHANNEL_MIN 172
#define CHANNEL_MAX 1811
// ===== in the Example there is a wave for the output channels that you can see the channes are moving
#define WAVE_SPEED 0.05
float wavePhase = 0.0;
#define STEP_INTERVAL 100   // 100ms Offset for each channel when the wave starts to the channel bevor

// ===== Logging Level =====
//#define LOG_LEVEL LOG_LEVEL_INFO   // oder INFO
#define LOG_LEVEL LOG_LEVEL_INFO
#include "logging.h"

// ======================================================
// 4. Global Objects
// ======================================================

unsigned long configWindowStart = 0;
uint16_t rampChannels[NUM_CHANNELS];
uint32_t lastStepTime = 0;
uint8_t activeChannel = 0;
bool rampUp = true;

// ======================================================
// CRSF Interface
// ======================================================
extern CRSF crsf;  // Instanziierung des CRSF-Objekts

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

// ======================================================
// ESP-NOW Receive Callback
// ======================================================
#if defined(ESP32)
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
LOG_DEBUG("ESPNow RX len=%d", len);

char hexBuffer[256];
int pos = 0;

for (int i = 0; i < len && pos < sizeof(hexBuffer) - 3; i++) {
    pos += snprintf(&hexBuffer[pos], sizeof(hexBuffer) - pos,
                    "%02X ", incomingData[i]);
}

LOG_DEBUG("Data: %s", hexBuffer);
#endif
  // For CRSF protocol from here
  espnow_len = len;
  crsf_len = len - 8;
  espnow_received = true;
  //memcpy(&crsf.crsf_buf, &*(incomingData+8), sizeof(crsf.crsf_buf));
  memcpy(&crsf.crsf_buf, incomingData + 8, sizeof(crsf.crsf_buf));

  // MSP Parsing
  for(int i = 0; i < len; i++) {
      if (recv_msp.processReceivedByte(incomingData[i])) {
        recv_msp.markPacketReceived();
      }
  }
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

void initWiFi()
{
    UID[0] &= ~0x01;   // unicast fix
    WiFi.mode(WIFI_STA);
    //WiFi.mode(WIFI_AP_STA); // Ermöglicht AP und Station gleichzeitig
    WiFi.disconnect();
    #if defined(ESP32)
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    #else
        wifi_set_channel(1);
    #endif
}

void initMac()
{
#if defined(ESP32)
    esp_wifi_set_mac(WIFI_IF_STA, UID);
#else
    wifi_set_macaddr(STATION_IF, UID);
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
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, UID, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
            LOG_ERROR("Peer add failed");
    #else
        esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
        esp_now_add_peer(UID, ESP_NOW_ROLE_COMBO, 0, NULL, 0);
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
    LOG_INFO("Config window active for %lu ms", CONFIG_WINDOW_MS);
    LOG_INFO("Send via Serial:");
    LOG_INFO("  1 = RX_ONLY");
    LOG_INFO("  2 = TX_ONLY");
    LOG_INFO("  3 = BOTH");
    configWindowStart = millis();
}

void setup() {
    initSerial();
    initWiFi();
    initMac();
    initESP32Queue();
    initESPNow();
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
    // Regular Mode
    // ======================================================
    if (radioMode != MODE_RX_ONLY)
    {
        vrxModule.updateChannelRamp();   // ESP-NOW senden
    }

    if (radioMode != MODE_TX_ONLY)
    {
        crsfReceive();                   // ESP-NOW empfangen
    }
}