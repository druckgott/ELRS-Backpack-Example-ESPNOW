#pragma once
#ifndef CRSF_H
#define CRSF_H

#include <Arduino.h>
#include <terseCRSF.h>  // https://github.com/zs6buj/terseCRSF   use v 0.0.6 or later
// crsf.h

// Deklarationen der Variablen
extern int16_t espnow_len;
extern int16_t crsf_len;
extern bool espnow_received;

// =======================
// RC Channels (NEU)
// =======================
#define CHANNELS_ID 0x16
#define MAX_CHANNELS 16
extern uint16_t rc_channels[MAX_CHANNELS];

// Hier die Prototypen der Funktionen und Deklarationen der Variablen
void crsfReceive();  // Beispiel für eine Funktion

#endif