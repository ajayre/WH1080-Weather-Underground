/* Module for uploading observations to Weather Underground
  (C) britishideas.com
  GPL V2 - see license.txt */

#ifndef _WUNDERGROUNDH_
#define _WUNDERGROUNDH_

// initializes the module
extern void WUnderground_Init
  (
  void
  );

// submits an observation
extern void WUnderground_Observation
  (
  double TemperatureC,                   // temperature in C
  double Humidity,                       // as a percentage
  double WindAveMph,
  double WindGustMph,
  char *WindDirection,                   // string "N", "NNE", "NE", "ENE", "E", etc.
  double TotalRainMm,                    // total since battery change
  double PressureHpa                     // pressure at sea level
  );

#endif // _WUNDERGROUNDH_

