/* Module for uploading observations to Weather Underground
  (C) britishideas.com
  GPL V2 - see license.txt */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <stdlib.h>
#include "wunderground.h"

// file that contains weather underground station ID (first line) and
// password (second line)
#define CREDENTIALS "wunderground_creds.txt"

// API url
#define WUURL "http://rtupdate.wunderground.com/weatherstation/updateweatherstation.php"

// maximum lengths of station id and password
#define STATIONIDLENGTH 50
#define PASSWORDLENGTH  50
// maximum command length
#define CMDLENGTH 1024

// number of samples of data per hour
// = 3600 seconds / 48 seconds
#define SAMPLESPERHOUR 75

// module variables
static char gStationId[STATIONIDLENGTH];
static char gPassword[PASSWORDLENGTH];

static int CurrentDay = -1;
static double RainatStartofDay = 0.0;

// keeps track of rain over the last hour
static int CurrentRainSample = -1;
static double LastRainSample = 0.0;
static double RainSamples[SAMPLESPERHOUR];

// gets rain in the last hour in mm
static double GetHourlyRainTotal
  (
  double TotalRain
  )
{
  int i;
  double Total;

  // no samples yet so initialize
  if (CurrentRainSample == -1)
  {
    LastRainSample = TotalRain;
    CurrentRainSample = 0;
    return 0.0;
  }

  // get new rainfail
  double NewRain = TotalRain - LastRainSample;
  LastRainSample = TotalRain;

  // store sample in circular buffer
  RainSamples[CurrentRainSample] = NewRain;
  CurrentRainSample++;
  if (CurrentRainSample == SAMPLESPERHOUR) CurrentRainSample = 0;

  // get total rain in last hour
  Total = 0.0;
  for (i = 0; i < SAMPLESPERHOUR; i++) Total += RainSamples[i];

  return Total;
}

// gets the total amount of rain for today in mm
static double GetDailyRainTotal
  (
  double TotalRain                    // total rain value from station
  )
{
  // get local time
  time_t t = time(NULL);
  struct tm *now = localtime(&t);

  // no daily rain information, so start anew
  if (CurrentDay == -1)
  {
    CurrentDay = now->tm_yday;
    RainatStartofDay = TotalRain;
    return 0.0;
  }

  // day has changed, start anew
  if (CurrentDay != now->tm_yday)
  {
    CurrentDay = now->tm_yday;
    RainatStartofDay = TotalRain;
    return 0.0;
  }

  // same day so get total rain since start of day
  double RainToday = TotalRain - RainatStartofDay;

  // if negative value then transmitter batteries were changed
  if (RainToday < 0.0)
  {
    RainatStartofDay = TotalRain;
    return 0.0;
  }

  return RainToday;
}

// initializes the module
void WUnderground_Init
  (
  void
  )
{
  int i;

  // read in credentials
  FILE *fp = fopen(CREDENTIALS, "r");
  fgets(gStationId, STATIONIDLENGTH, fp);
  strtok(gStationId, "\n");
  fgets(gPassword,  PASSWORDLENGTH,  fp);
  strtok(gPassword, "\n");
  fclose(fp);

  // no rain data
  for (i = 0; i < SAMPLESPERHOUR; i++) RainSamples[i] = 0.0;
}

// submits an observation
void WUnderground_Observation
  (
  double TemperatureC,                   // temperature in C
  double Humidity,                       // as a percentage
  double WindAveMph,
  double WindGustMph,
  char *WindDirection,                   // string "N", "NNE", "NE", "ENE", "E", etc.
  double TotalRainMm,                    // total since battery change
  double PressureHpa                     // pressure at sea level
  )
{
  char Cmd[CMDLENGTH];
  char UTCTimestamp[40];

  // get timestamp
  time_t now = time(NULL);
  strftime(UTCTimestamp, 40, "%Y-%m-%d+%H%%3A%M%%3A%S", gmtime(&now));

  // get temperature in F
  double TemperatureF = (TemperatureC * 9.0 / 5.0) + 32.0;

  // calculate dew point in F
  // see: http://www.gorhamschaffler.com/humidity_formulas.htm
  double Es = 6.11 * pow(10.0, 7.5 * TemperatureC / (237.7 + TemperatureC));
  double E = Humidity * Es / 100.0;
  double Tdc = (-430.22 + 237.7 * log(E)) / (-log(E) + 19.08);
  double DewPointF = (Tdc * 9.0 / 5.0) + 32.0;

  // get wind direction
  double WindDegrees = 0;
       if (!strcmp(WindDirection, "N"))   WindDegrees = 0;
  else if (!strcmp(WindDirection, "NNE")) WindDegrees = 22.5;
  else if (!strcmp(WindDirection, "NE"))  WindDegrees = 45;
  else if (!strcmp(WindDirection, "ENE")) WindDegrees = 67.5;
  else if (!strcmp(WindDirection, "E"))   WindDegrees = 90;
  else if (!strcmp(WindDirection, "ESE")) WindDegrees = 112.5;
  else if (!strcmp(WindDirection, "SE"))  WindDegrees = 135;
  else if (!strcmp(WindDirection, "SSE")) WindDegrees = 157.5;
  else if (!strcmp(WindDirection, "S"))   WindDegrees = 180;
  else if (!strcmp(WindDirection, "SSW")) WindDegrees = 202.5;
  else if (!strcmp(WindDirection, "SW"))  WindDegrees = 225;
  else if (!strcmp(WindDirection, "WSW")) WindDegrees = 247.5;
  else if (!strcmp(WindDirection, "W"))   WindDegrees = 270;
  else if (!strcmp(WindDirection, "WNW")) WindDegrees = 292.5;
  else if (!strcmp(WindDirection, "NW"))  WindDegrees = 315;
  else if (!strcmp(WindDirection, "NNW")) WindDegrees = 337.5;

  // get amount of rain today in mm
  double RainToday = GetDailyRainTotal(TotalRainMm);
  // get amount of rain in last hour in mm
  double RainLastHour = GetHourlyRainTotal(TotalRainMm);

  sprintf(Cmd, "wget -b -a /tmp/wunderground.log -O /tmp/wunderground-result.html \"%s?action=updateraw&ID=%s&PASSWORD=%s&realtime=1&rtfreq=48&dateutc=%s&tempf=%f&humidity=%f&windspeedmph=%f&windgustmph=%f&baromin=%f,&dewptf=%f&winddir=%.1f&dailyrainin=%f&rainin=%f\"",
    WUURL,
    gStationId,
    gPassword,
    UTCTimestamp,
    TemperatureF,
    Humidity,
    WindAveMph,
    WindGustMph,
    PressureHpa * 0.0295299830714,
    DewPointF,
    WindDegrees,
    RainToday * 0.0393701,
    RainLastHour * 0.0393701
    );

  printf("%s\n", Cmd);
  system(Cmd);
}
