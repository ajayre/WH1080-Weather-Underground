WH1080-Weather-Underground
==========================

Raspberry Pi bridge between a WH1080 weather station and the Weather Underground PWS network

95% based on Kevin Sangeelee's work at http://www.susa.net/wordpress/2012/08/raspberry-pi-reading-wh1081-weather-sensors-using-an-rfm01-and-rfm12b/

See also:

  http://www.raspberrypi.org/forums/viewtopic.php?p=152023
  http://www.quasaruk.co.uk/acatalog/RFM01.pdf

Works on a Raspberry Pi V2 with an RFM01 module.

Module connections:

RFM01   ->   RPi    (pin)
=========================
SDO          MISO   (21)
SDI          MOSI   (19)
SCK          SCLK   (23)
nSEL         CE0_N  (24)
DATA/nFFS    GPIO27 (13)
VDD          3V3    (1)
GND          GND    (6)


17.3cm wire connected to ANT
