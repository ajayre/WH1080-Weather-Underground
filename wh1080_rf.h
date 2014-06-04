
//#define USE_BMP085
//#define ALTITUDE_M	210.0f
#define ALTITUDE_M	6.0f

void scheduler_realtime();
void scheduler_standard();
void calculate_values(unsigned char *buf, float pressure_hpa);
