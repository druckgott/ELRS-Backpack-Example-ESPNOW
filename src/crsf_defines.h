#ifndef CRSF_FRAME_TYPES_H
#define CRSF_FRAME_TYPES_H

// CRSF Frame Types
#define CRSF_FRAMETYPE_GPS              0x02
#define CRSF_FRAMETYPE_VARIO            0x0/
#define CRSF_FRAMETYPE_BATTERY          0x08
#define CRSF_FRAMETYPE_ATTITUDE         0x1E
#define CRSF_FRAMETYPE_FLIGHT_MODE      0x21
#define CRSF_FRAMETYPE_LINK_STATISTICS  0x14

// WICHTIG:
#define CRSF_FRAMETYPE_RC_CHANNELS      0x16
#define CRSF_FRAMETYPE_CMD              0x32

// Falls noch nicht vorhanden:
#define CRSF_FRAMETYPE_TELEMETRY        0x00  // nur falls dein Stack das so nutzt

#endif