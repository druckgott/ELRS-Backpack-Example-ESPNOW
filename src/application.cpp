#include "application.h"
#include "terseCRSF.h"
#include "logging.h"
#include "types.h"
#include "crsf_defines.h"
#include <math.h>
#include <string>

extern CRSF crsf;

extern int16_t hud_bat1_volts;
extern int16_t hud_bat1_amps;
extern uint16_t hud_bat1_mAh;

extern bool motArmed;

extern Location hom;
extern Location cur;

extern bool finalHomeStored;

/* ===================== */
/*        STATES         */
/* ===================== */

static bool prevGpsGood = false;
static std::string lastMode = "";

uint16_t channel[16];

/* ===================== */
/*     CRSF PROCESSING   */
/* ===================== */

void processCRSFFrame(uint8_t* buffer, uint16_t len)
{

    // ================= RC CHANNELS =================
    if (buffer[2] == CRSF_FRAMETYPE_RC_CHANNELS)   // CRSF_FRAMETYPE_RC_CHANNELS
    {
        LOG_INFO("RC Frame received (%d bytes)", len);
        crsf.decodeRC();
        return;
    }

    uint8_t crsf_id = crsf.decodeTelemetry(buffer, len);

    switch (crsf_id)
    {

        

        /* ================= GPS ================= */
        case GPS_ID:
        {
            cur.lat = crsf.gpsF_lat;
            cur.lon = crsf.gpsF_lon;
            cur.alt = crsf.gps_altitude;

            bool gpsfixGood = (crsf.gps_sats >= 5);
            bool lonGood    = (crsf.gpsF_lon != 0.0f);
            bool latGood    = (crsf.gpsF_lat != 0.0f);
            bool altGood    = (crsf.gps_altitude != 0);

            bool gpsGood = gpsfixGood && lonGood && latGood && altGood;

            if (finalHomeStored)
                cur.alt_ag = cur.alt - hom.alt;
            else
                cur.alt_ag = 0;

            /* ---- GPS LOG ---- */
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

            /* ---- GPS STATUS CHANGE ---- */
            if (gpsGood != prevGpsGood)
            {
                LOG_INFO("GPS Status changed: gpsGood=%d", gpsGood);
                prevGpsGood = gpsGood;
            }

        } break;

        /* ================= VARIO ================= */
        case CF_VARIO_ID:
        {
            // crsf.decodeTelemetry(buffer, len) hat bereits crsf.vario / crsf.varioF gesetzt

            // rohdaten in cm/s
            int16_t climb_cm = crsf.vario;

            // in m/s für Berechnungen / Logging
            float climb_m_s = crsf.varioF;

            static float lastClimb = 999.0f;

            // nur loggen, wenn sich Wert geändert hat
            if (fabs(climb_m_s - lastClimb) > 0.01f)
            {
                LOG_INFO("VARIO: %+.2f m/s (%d cm/s)", climb_m_s, climb_cm);
                lastClimb = climb_m_s;
            }

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

        /* ================= BAROMETER ================= */
        case BARO_ALT_ID:
        {
            // Rohwert aus CRSF-Klasse
            uint16_t alt_raw = crsf.baro_altitude;
            LOG_INFO("BARO: raw=%u", alt_raw);

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

                /* ================= LINK STATISTICS (0x14) ================= */
        case LINK_ID:   // 0x14
        {
            LOG_INFO("LINK: RSSI1=%d RSSI2=%d Q=%d SNR=%d TX=%d RF=%d",
                     crsf.link_up_rssi_ant_1,
                     crsf.link_up_rssi_ant_2,
                     crsf.link_up_quality,
                     crsf.link_up_snr,
                     crsf.link_up_tx_power,
                     crsf.link_rf_mode);
        } break;

        /* ================= FLIGHT MODE ================= */
        case FLIGHT_MODE_ID:
        {
            motArmed = (crsf.flightMode.compare("ARM") == 0);

            if (crsf.flightMode != lastMode)
            {
                LOG_INFO("MODE: %s Armed:%d",
                         crsf.flightMode.c_str(),
                         motArmed);

                lastMode = crsf.flightMode;
            }

        } break;

        default:
        {
            LOG_INFO("Unknown CRSF ID: 0x%02X (len=%d)", crsf_id, len);

            // komplettes Paket als HEX dump ausgeben
            char hexString[512] = {0};
            char* ptr = hexString;

            for (uint16_t i = 0; i < len; i++)
            {
                ptr += sprintf(ptr, "%02X ", buffer[i]);
            }

            LOG_INFO("Frame Data: %s", hexString);
        }
        break;
    }
}