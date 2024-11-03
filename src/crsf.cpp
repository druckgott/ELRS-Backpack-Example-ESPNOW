#include <Arduino.h>
#include <terseCRSF.h>  // https://github.com/zs6buj/terseCRSF   use v 0.0.6 or later
// crsf.cpp
#include "crsf.h"

CRSF crsf;  // Instanziierung des CRSF-Objekts

int16_t  espnow_len = 0;
int16_t  crsf_len = 0;
bool espnow_received = false;

uint8_t  hud_num_sats = 0; 
float    hud_grd_spd = 0;
int16_t  hud_bat1_volts = 0;     // dV (V * 10)
int16_t  hud_bat1_amps = 0;      // dA )A * 10)
uint16_t hud_bat1_mAh = 0;

struct Location {
  float lat; //long
  float lon;
  float alt;
  float hdg;
  float alt_ag;     
};

struct Location hom     = {
  0,0,0,0
};   // home location
struct Location cur      = {
  0,0,0,0
};   // current location

bool gpsGood = false; 
bool gpsPrev = false; 
bool hdgGood = false;       
bool serGood = false; 
bool lonGood = false;
bool latGood = false;
bool altGood = false;               
bool boxhdgGood = false; 
bool motArmed = false;   // motors armed flag
bool gpsfixGood = false;
 
bool finalHomeStored = false;

uint32_t  gpsGood_millis = 0;

void crsfReceive() 
{
  if (espnow_received)  // flag
  {
    espnow_received = false;
    int16_t len = espnow_len;
    uint8_t crsf_id = crsf.decodeTelemetry(&*crsf.crsf_buf, len);
    //log.printf("crsf_id:%2X\n", crsf_id);
    if (crsf_id == GPS_ID)   // 0x02
    {
      // don't use gps heading, use attitude yaw below
      cur.lon = crsf.gpsF_lon;
      cur.lat = crsf.gpsF_lat;    
      cur.alt = crsf.gps_altitude;
      gpsfixGood = (crsf.gps_sats >=5);  // with 4 sats, altitude value can be bad
      lonGood = (crsf.gpsF_lon != 0.0);
      latGood = (crsf.gpsF_lat != 0.0);
      altGood = (crsf.gps_altitude != 0);
      hdgGood = true; 
      if ((finalHomeStored))
      {
        cur.alt_ag = cur.alt - hom.alt;
      } else 
      {
        cur.alt_ag = 0;
      }           
      hud_num_sats = crsf.gps_sats;         // these for the display
      hud_grd_spd = crsf.gpsF_groundspeed;   
      log.print("GPS id:");
      crsf.printByte(crsf_id, ' ');
      log.printf("lat:%2.7f  lon:%2.7f", crsf.gpsF_lat, crsf.gpsF_lon);
      log.printf("  ground_spd:%.1fkm/hr", crsf.gpsF_groundspeed);
      log.printf("  hdg:%.2fdeg", crsf.gpsF_heading);
      log.printf("  alt:%dm", crsf.gps_altitude);
      log.printf("  sats:%d", crsf.gps_sats); 
      log.printf("  gpsfixGood:%d", gpsfixGood); 
      log.printf("  hdgGood:%d\n", hdgGood);          
    }
    if (crsf_id == BATTERY_ID) 
    { 
      hud_bat1_volts = crsf.batF_voltage;           
      hud_bat1_amps = crsf.batF_current;
      hud_bat1_mAh = crsf.batF_fuel_drawn * 1000;       
      log.print("BATTERY id:");
      crsf.printByte(crsf_id, ' ');
      log.printf("volts:%2.1f", crsf.batF_voltage);
      log.printf("  amps:%3.1f", crsf.batF_current);
      log.printf("  Ah_drawn:%3.1f", crsf.batF_fuel_drawn);
     log.printf("  remaining:%3u%%\n", crsf.bat_remaining);
    }
   
    if (crsf_id == ATTITUDE_ID)
    {
      cur.hdg = crsf.attiF_yaw;
      log.print("ATTITUDE id:");
      crsf.printByte(crsf_id, ' '); 
      log.printf("pitch:%3.1fdeg", crsf.attiF_pitch);
      log.printf("  roll:%3.1fdeg", crsf.attiF_roll);
      log.printf("  yaw:%3.1fdeg\n", crsf.attiF_yaw);           
    }    

    if (crsf_id == FLIGHT_MODE_ID)
    {
      motArmed = !(crsf.flightMode.compare("ARM"));  // Returns 0 if both the strings are the same
      log.print("FLIGHT_MODE id:");
      crsf.printByte(crsf_id, ' ');
      log.printf("lth:%u %s motArmed:%u\n", crsf.flight_mode_lth, crsf.flightMode.c_str(), motArmed);
    gpsGood = (gpsfixGood & lonGood & latGood & altGood);    
    if (gpsGood) gpsGood_millis = millis();     // Time of last good GPS packet 
    }
  }      
  
  static bool prevGpsGood = 0;
  if (gpsGood != prevGpsGood)
  {
    log.printf("gpsGood:%u  gpsfixGood:%u  lonGood:%u  latGood:%u  altGood:%u  hdgGood:%u  boxhdgGood:%u \n", gpsGood, gpsfixGood, lonGood, latGood, altGood, hdgGood, boxhdgGood);  
    prevGpsGood = gpsGood;
  }      
}     