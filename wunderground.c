/* Module for uploading observations to Weather Underground
  (C) britishideas.com 2014

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <stdlib.h>
#include "wunderground.h"

// file that contains weather underground station ID (first line) and
// password (second line)
#define CREDENTIALS "/home/pi/weather/wh1080wunderground/trunk/wunderground_creds.txt"

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

// number of wind direction vector to use for averaging
#define WINDVECTORS 16

// number of unique wind directions we support
#define WINDDIRECTIONS 16

// minimum speed of wind in mph before we start using the wind direction for
// averaging
#define MINIMUMWINDSPEED 0.1

#define PI 3.14159265359

// module variables
static char gStationId[STATIONIDLENGTH];
static char gPassword[PASSWORDLENGTH];

static int CurrentDay = -1;
static double RainatStartofDay = 0.0;

// keeps track of rain over the last hour
static int CurrentRainSample = -1;
static double LastRainSample = 0.0;
static double RainSamples[SAMPLESPERHOUR];

// keeps track of wind vectors (direction and speed)
static int CurrentWindVector = 0;
static int WindDirectionAveragingEnabled = 0;
static double LastAveWindDegrees = 0.0;
static int WindVectors_Dir[WINDVECTORS];
static double WindVectors_Speed[WINDVECTORS];

// gets the average wind direction in degrees based on previous wind vectors
// mostly adapted from pywws:
// https://github.com/jim-easterbrook/pywws see pywws/Process.py
static double GetAverageWindDirection
  (
  )
{
  double WeightedVectors[WINDDIRECTIONS];
  int i;
  int dir;
  int DeadCalm = 1;

  // initialize weighted vectors
  for (dir = 0; dir < WINDDIRECTIONS; dir++) WeightedVectors[dir] = 0.0;

  // set up weighted vectors
  for (i = 0; i < WINDVECTORS; i++)
  {
    if (WindVectors_Speed[i] > MINIMUMWINDSPEED)
    {
      WeightedVectors[WindVectors_Dir[i]] += WindVectors_Speed[i];
      DeadCalm = 0;
    }
  }

  // if all wind vectors show no wind then use the last average we worked out
  if (DeadCalm)
  {
    return LastAveWindDegrees;
  }

  // convert weighted vectors into a single average
  double Ve = 0.0;
  double Vn = 0.0;
  for (dir = 0; dir < WINDDIRECTIONS; dir++)
  {
    Ve -= WeightedVectors[dir] * sin((dir * 360.0 / WINDDIRECTIONS) * PI / 180.0);
    Vn -= WeightedVectors[dir] * cos((dir * 360.0 / WINDDIRECTIONS) * PI / 180.0);
  }
  double AveDir = ((atan2(Ve, Vn) * 180.0 / PI) + 180.0) * WINDDIRECTIONS / 360.0;
  AveDir = ((int)(AveDir + 0.5) % WINDDIRECTIONS) * (360.0 / WINDDIRECTIONS);

  return AveDir;
}

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
  // no wind vectors
  for (i = 0; i < WINDVECTORS; i++)
  {
    WindVectors_Dir[i] = 0;
    WindVectors_Speed[i] = 0.0;
  }
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
  double WindIndex = 0;
       if (!strcmp(WindDirection, "N"))   WindIndex = 0;
  else if (!strcmp(WindDirection, "NNE")) WindIndex = 1;
  else if (!strcmp(WindDirection, "NE"))  WindIndex = 2;
  else if (!strcmp(WindDirection, "ENE")) WindIndex = 3;
  else if (!strcmp(WindDirection, "E"))   WindIndex = 4;
  else if (!strcmp(WindDirection, "ESE")) WindIndex = 5;
  else if (!strcmp(WindDirection, "SE"))  WindIndex = 6;
  else if (!strcmp(WindDirection, "SSE")) WindIndex = 7;
  else if (!strcmp(WindDirection, "S"))   WindIndex = 8;
  else if (!strcmp(WindDirection, "SSW")) WindIndex = 9;
  else if (!strcmp(WindDirection, "SW"))  WindIndex = 10;
  else if (!strcmp(WindDirection, "WSW")) WindIndex = 11;
  else if (!strcmp(WindDirection, "W"))   WindIndex = 12;
  else if (!strcmp(WindDirection, "WNW")) WindIndex = 13;
  else if (!strcmp(WindDirection, "NW"))  WindIndex = 14;
  else if (!strcmp(WindDirection, "NNW")) WindIndex = 15;

  // calculate degrees for wind direction
  double WindDegrees = WindIndex * 22.5;

  // store wind vector in circular buffer
  WindVectors_Dir[CurrentWindVector] = WindIndex;
  WindVectors_Speed[CurrentWindVector] = WindAveMph;
  CurrentWindVector++;
  if (CurrentWindVector == WINDVECTORS)
  {
    CurrentWindVector = 0;
    // now have full set of samples so can use average
    WindDirectionAveragingEnabled = 1;
  }

  // if wind direction averaging enabled then get the average direction
  // if not enabled then use current wind direction instead
  double AveWindDegrees = WindDegrees;
  if (WindDirectionAveragingEnabled)
  {
    AveWindDegrees = GetAverageWindDirection();
    printf("Average wind direction = %f\n", AveWindDegrees);
  }

  // remember last average wind direction
  LastAveWindDegrees = AveWindDegrees;

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
    AveWindDegrees,
    RainToday * 0.0393701,
    RainLastHour * 0.0393701
    );

  printf("%s\n", Cmd);
  system(Cmd);
}
