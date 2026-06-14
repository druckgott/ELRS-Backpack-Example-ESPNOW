#include "fake_vrx_fake_trainer.h"
#include "logging.h"

// Wichtig für den ESP32 Raw-Bypass
#if defined(ESP32)
#include <esp_wifi.h>
#endif

extern MSP recv_msp;

void FakeVRXFakeTrainer::init(const uint8_t* uid)
{
    memcpy(_uid, uid, 6);
}

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
    // --- ESP32 HARDWARE L2 BYPASS ---
    uint8_t raw_frame[256];
    size_t payload_len = size; 
    if (payload_len > 200) payload_len = 200;

    // 1. IEEE 802.11 MAC Header (Action Frame)
    raw_frame[0] = 0xD0; raw_frame[1] = 0x00;
    raw_frame[2] = 0x00; raw_frame[3] = 0x00;
    
    // ExpressLRS "Inverted Addressing": Ziel, Quelle und BSSID sind die UID
    memcpy(&raw_frame[4],  _uid, 6); 
    memcpy(&raw_frame[10], _uid, 6); 
    memcpy(&raw_frame[16], _uid, 6); 
    
    raw_frame[22] = 0x00; raw_frame[23] = 0x00;

    // 2. Espressif Vendor-Specific Header (OUI: 18:FE:34)
    raw_frame[24] = 0x7F; 
    raw_frame[25] = 0x18; raw_frame[26] = 0xFE; raw_frame[27] = 0x34; 

    // 3. Nutzdaten kopieren
    memcpy(&raw_frame[28], buf, payload_len);
    size_t frame_len = 28 + payload_len;

    // 4. Promiscuous Mode kurz aktivieren, um Treiber-Filter zu umgehen
    bool was_promisc = false;
    esp_wifi_get_promiscuous(&was_promisc);
    
    int result = esp_wifi_80211_tx(WIFI_IF_STA, raw_frame, frame_len, true);
    
    if (!was_promisc) esp_wifi_set_promiscuous(false);

    if (result != ESP_OK) LOG_WARN("esp_wifi_80211_tx failed (%d)", result);
#else
    // --- ESP8266 STANDARD PATH ---
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
    // --- ESP32 HARDWARE L2 BYPASS ---
    uint8_t raw_frame[256];
    size_t payload_len = size; 
    if (payload_len > 200) payload_len = 200;

    // 1. IEEE 802.11 MAC Header (Action Frame)
    raw_frame[0] = 0xD0; raw_frame[1] = 0x00;
    raw_frame[2] = 0x00; raw_frame[3] = 0x00;
    
    // ExpressLRS "Inverted Addressing"
    memcpy(&raw_frame[4],  _uid, 6); 
    memcpy(&raw_frame[10], _uid, 6); 
    memcpy(&raw_frame[16], _uid, 6); 
    
    raw_frame[22] = 0x00; raw_frame[23] = 0x00;

    // 2. Espressif Vendor-Specific Header
    raw_frame[24] = 0x7F; 
    raw_frame[25] = 0x18; raw_frame[26] = 0xFE; raw_frame[27] = 0x34; 

    // 3. Nutzdaten kopieren
    memcpy(&raw_frame[28], buf, payload_len);
    size_t frame_len = 28 + payload_len;

    // 4. In die Antenne injizieren
    bool was_promisc = false;
    esp_wifi_get_promiscuous(&was_promisc);
    
    int result = esp_wifi_80211_tx(WIFI_IF_STA, raw_frame, frame_len, true);
    
    if (!was_promisc) esp_wifi_set_promiscuous(false);

    if (result != ESP_OK) LOG_WARN("esp_wifi_80211_tx failed (%d)", result);
#else
    // --- ESP8266 STANDARD PATH ---
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