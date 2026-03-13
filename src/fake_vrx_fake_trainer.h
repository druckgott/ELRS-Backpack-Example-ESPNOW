#pragma once

#include <Arduino.h>
#include "msp.h"
#include "msptypes.h"

#if defined(ESP32)
#include <esp_now.h>
#else
#include <espnow.h>
#endif

class FakeVRXFakeTrainer
{
public:
    void init(const uint8_t* uid);

    void sendFakeHeadtracking(uint16_t pan, uint16_t roll, uint16_t tilt);
    void sendTrainerMode16ch(uint16_t *channels);
    void updateChannelRamp();

private:
    uint8_t _uid[6];

    uint16_t rampChannels[16];

    uint32_t lastStepTime = 0;
    float wavePhase = 0.0f;

    static constexpr uint8_t NUM_CHANNELS = 16;
    static constexpr uint16_t CHANNEL_MIN = 172;
    static constexpr uint16_t CHANNEL_MAX = 1811;
    static constexpr float WAVE_SPEED = 0.05f;
    static constexpr uint32_t STEP_INTERVAL = 100;
};