#include "fake_vrx_fake_trainer.h"
#include "logging.h"

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

    int result = esp_now_send(_uid, buf, size);

    if (result != 0)
        LOG_WARN("esp_now_send failed (%d)", result);
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

    int result = esp_now_send(_uid, buf, size);

    if (result != 0)
        LOG_WARN("esp_now_send failed (%d)", result);
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