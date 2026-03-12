// crsf.cpp
#include "crsf.h"
#include "logging.h"
#include <math.h>
#include <string>

CRSF crsf;

int16_t espnow_len = 0;
int16_t crsf_len = 0;
bool espnow_received = false;

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

void crsfReceive()
{
    if (espnow_len <= 0)
        return;

    int16_t len = espnow_len;
    espnow_len = 0;

    uint8_t crsf_id = crsf.decodeTelemetry(crsf.crsf_buf, len);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DEBUG("CRSF ID: 0x%02X", crsf_id);
#endif

    switch (crsf_id)
    {
        /* ================= GPS ================= */
        case GPS_ID:
        {
            cur.lat = crsf.gpsF_lat;
            cur.lon = crsf.gpsF_lon;
            cur.alt = crsf.gps_altitude;

            gpsfixGood = (crsf.gps_sats >= 5);
            lonGood = (crsf.gpsF_lon != 0.0f);
            latGood = (crsf.gpsF_lat != 0.0f);
            altGood = (crsf.gps_altitude != 0);

            gpsGood = gpsfixGood && lonGood && latGood && altGood;

            if (finalHomeStored)
                cur.alt_ag = cur.alt - hom.alt;
            else
                cur.alt_ag = 0;

            hud_num_sats = crsf.gps_sats;
            hud_grd_spd  = crsf.gpsF_groundspeed;

            static float lastLat = 999;
            static float lastLon = 999;

            if (fabs(cur.lat - lastLat) > 0.000001f ||
                fabs(cur.lon - lastLon) > 0.000001f)
            {
                LOG_INFO("GPS: %.7f, %.7f alt=%d sats=%d",
                         cur.lat,
                         cur.lon,
                         crsf.gps_altitude,
                         crsf.gps_sats);

                lastLat = cur.lat;
                lastLon = cur.lon;
            }

            if (gpsGood)
                gpsGood_millis = millis();

        } break;

        /* ================= BATTERY ================= */
        case BATTERY_ID:
        {
            hud_bat1_volts = crsf.batF_voltage;
            hud_bat1_amps  = crsf.batF_current;
            hud_bat1_mAh   = crsf.batF_fuel_drawn * 1000;

            static int16_t lastVolt = -9999;

            if (hud_bat1_volts != lastVolt)
            {
                LOG_INFO("BAT: %.1fV %.1fA %u%%",
                         crsf.batF_voltage,
                         crsf.batF_current,
                         crsf.bat_remaining);

                lastVolt = hud_bat1_volts;
            }

        } break;

        /* ================= ATTITUDE ================= */
        case ATTITUDE_ID:
        {
            cur.hdg = crsf.attiF_yaw;

            static float lastPitch = 999;

            if (fabs(crsf.attiF_pitch - lastPitch) > 0.1f)
            {
                LOG_INFO("ATT: p=%.1f r=%.1f y=%.1f",
                         crsf.attiF_pitch,
                         crsf.attiF_roll,
                         crsf.attiF_yaw);

                lastPitch = crsf.attiF_pitch;
            }

        } break;

        /* ================= FLIGHT MODE ================= */
        case FLIGHT_MODE_ID:
        {
            motArmed = (crsf.flightMode.compare("ARM") == 0);

            static std::string lastMode = "";

            if (crsf.flightMode != lastMode)
            {
                LOG_INFO("MODE: %s Armed:%d",
                         crsf.flightMode.c_str(),
                         motArmed);

                lastMode = crsf.flightMode;
            }

            gpsGood = (gpsfixGood && lonGood && latGood && altGood);

        } break;

        default:
            break;
    }

    /* ================= GPS STATUS CHANGE ================= */

    static bool prevGpsGood = false;

    if (gpsGood != prevGpsGood)
    {
        LOG_INFO("GPS Status changed: gpsGood=%d",
                 gpsGood);

        prevGpsGood = gpsGood;
    }
}