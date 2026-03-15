// crsf.cpp
#include "logging.h"
#include "crsf.h"
#include <math.h>
#include <string>

uint16_t rc_channels[MAX_CHANNELS] = {0};

uint8_t  hud_num_sats = 0;
float    hud_grd_spd = 0;
int16_t  hud_bat1_volts = 0;
int16_t  hud_bat1_amps = 0;
uint16_t hud_bat1_mAh = 0;

struct Location {
    float lat;
    float lon;
    float alt;
    float hdg;
    float alt_ag;
};

Location hom = {0,0,0,0,0};
Location cur = {0,0,0,0,0};

bool gpsGood = false;
bool gpsfixGood = false;
bool lonGood = false;
bool latGood = false;
bool altGood = false;
bool hdgGood = false;
bool motArmed = false;
bool finalHomeStored = false;

uint32_t gpsGood_millis = 0;

/* ========================================================= */

