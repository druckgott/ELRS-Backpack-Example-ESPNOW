#include "fake_vrx_fake_trainer.h"
#include "logging.h"

#if defined(ESP32)
#include <Arduino.h>
#include <esp_wifi.h>
#endif

extern MSP recv_msp;

void FakeVRXFakeTrainer::init(const uint8_t* uid)
{
    memcpy(_uid, uid, 6);
}

#if defined(ESP32)
void printPacketDump(const char* label, const uint8_t* dest_mac, const uint8_t* data, uint8_t len) {
    // Greift auf dein #define LOG_LEVEL LOG_LEVEL_DEBUG aus der logging.h zu
    #if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Serial.print("[DBG] [PACKET DUMP] ");
        Serial.print(label);
        Serial.print(" | Dest MAC: ");
        for(int i=0; i<6; i++) {
            Serial.printf("%02X", dest_mac[i]);
            if(i<5) Serial.print(":");
        }
        Serial.printf(" | Len: %d | Payload: ", len);
        for(int i=0; i<len; i++) {
            Serial.printf("%02X ", data[i]);
        }
        Serial.println();
    #endif
}

esp_err_t custom_esp_now_send(const uint8_t *peer_mac, const uint8_t *data, size_t len) {
    uint8_t raw_frame[250];

    // 0-3: Frame Control & Duration
    raw_frame[0] = 0xD0;
    raw_frame[1] = 0x00;
    raw_frame[2] = 0x3C; 
    raw_frame[3] = 0x00;

    // 4-9: Destination MAC (UID)
    memcpy(&raw_frame[4], peer_mac, 6);

    // 10-15: Source MAC (UID)
    memcpy(&raw_frame[10], peer_mac, 6);

    // 16-21: BSSID (UID)
    memcpy(&raw_frame[16], peer_mac, 6);

    // 22-23: Sequence Control
    static uint16_t seq = 0;
    raw_frame[22] = (seq & 0x0F) << 4;
    raw_frame[23] = (seq >> 4);
    seq++;

    // 24-27: Category & OUI
    raw_frame[24] = 0x7F;
    raw_frame[25] = 0x18;
    raw_frame[26] = 0xFE;
    raw_frame[27] = 0x34;

    // 28-31: HIER IST DER FIX! Dynamischer Anti-Replay-Schutz
    // Wenn das statisch bleibt, droppt die Anlage alles als Duplikat.
    uint32_t magic = esp_random(); // Generiert jedes Mal neue Bytes
    raw_frame[28] = magic & 0xFF; 
    raw_frame[29] = (magic >> 8) & 0xFF;
    raw_frame[30] = (magic >> 16) & 0xFF;
    raw_frame[31] = (magic >> 24) & 0xFF;

    // 32-38: Information Element (ESP-NOW Typ)
    raw_frame[32] = 0xDD;
    raw_frame[33] = len + 5; 
    raw_frame[34] = 0x18;
    raw_frame[35] = 0xFE;
    raw_frame[36] = 0x34;
    raw_frame[37] = 0x04;
    raw_frame[38] = 0x01;

    // 39+: Payload
    memcpy(&raw_frame[39], data, len);

    size_t frame_len = 39 + len;

    // Zwinge das WLAN-Modul direkt vor dem Schuss hart auf Kanal 1
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    // Auf die Antenne feuern
    bool was_promisc = false;
    esp_wifi_get_promiscuous(&was_promisc);
    if (!was_promisc) esp_wifi_set_promiscuous(true);

    // WICHTIG: Das 'false' am Ende verbietet der ESP32-Hardware, 
    // unsere manuellen Sequence-Bytes (22-23) zu überschreiben!
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, raw_frame, frame_len, true);

    if (!was_promisc) esp_wifi_set_promiscuous(false);

    return result;
}
#endif

void FakeVRXFakeTrainer::sendFakeHeadtracking(uint16_t pan, uint16_t roll, uint16_t tilt)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_PTR;

    packet.addByte(pan & 0xFF);
    packet.addByte(pan >> 8);

    packet.addByte(roll & 0xFF);
    packet.addByte(roll >> 8);

    packet.addByte(tilt & 0xFF);
    packet.addByte(tilt >> 8);

    uint8_t buf[64];
    uint8_t size = recv_msp.convertToByteArray(&packet, buf);

#if defined(ESP32)
    printPacketDump("ESP32-TX", _uid, buf, size);
    int result = custom_esp_now_send(_uid, buf, size);
    
    if (result != ESP_OK) {
        LOG_WARN("custom_esp_now_send failed (%d)", result);
    } else {
        // Hier ist das visuelle Feedback für dein Terminal!
        LOG_DEBUG("ESP-NOW Send OK (Bypass)   \n");
    }
#else
    int result = esp_now_send(_uid, buf, size);

    if (result != 0)
        LOG_WARN("esp_now_send failed (%d)", result);
#endif
}

void FakeVRXFakeTrainer::sendTrainerMode16ch(uint16_t *channels)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_BACKPACK_SET_PTR;

    for(int i = 0; i < NUM_CHANNELS; i++)
    {
        packet.addByte(channels[i] & 0xFF);
        packet.addByte(channels[i] >> 8);
    }

    uint8_t buf[64];
    uint8_t size = recv_msp.convertToByteArray(&packet, buf);

#if defined(ESP32)
    printPacketDump("ESP32-TX", _uid, buf, size);
    int result = custom_esp_now_send(_uid, buf, size);
    
    if (result != ESP_OK) {
        LOG_WARN("custom_esp_now_send failed (%d)", result);
    } else {
        // Hier ist das visuelle Feedback für dein Terminal!
        LOG_DEBUG("ESP-NOW Send OK (Bypass)   \n");
    }
#else
    int result = esp_now_send(_uid, buf, size);

    if (result != 0)
        LOG_WARN("esp_now_send failed (%d)", result);
#endif
}

void FakeVRXFakeTrainer::updateChannelRamp()
{
    uint32_t now = millis();

    if (now - lastStepTime < STEP_INTERVAL)
        return;

    lastStepTime = now;

    for(int i = 0; i < NUM_CHANNELS; i++)
    {
        float phase = wavePhase + i * 0.4f;
        float s = sin(phase);
        float normalized = (s + 1.0f) * 0.5f;

        rampChannels[i] = CHANNEL_MIN +
            normalized * (CHANNEL_MAX - CHANNEL_MIN);
    }

    wavePhase += WAVE_SPEED;

    sendTrainerMode16ch(rampChannels);
}