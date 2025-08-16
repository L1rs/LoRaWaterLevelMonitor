// Deutsche Dokumentation
// Implementierung der Mess- und Umrechnungsfunktionen
#include "measurement.h"
#include <Arduino.h>
#include "config.h"

uint32_t readMilliVoltsAveraged(int pin, int samples)
{
    uint64_t acc = 0;
    for (int i = 0; i < samples; ++i) {
        acc += analogReadMilliVolts(pin);
        delayMicroseconds(200);
    }
    return (uint32_t)(acc / samples);
}

float mvToDepthCm(uint32_t mv)
{
    float mvCal = (float)mv * SENSOR_MV_SCALE;
    int32_t mvAdj = (int32_t)mvCal - (int32_t)SENSOR_OFFSET_MV;
    if (mvAdj < 0) mvAdj = 0;
    return ((float)mvAdj / (float)SENSOR_VREF_MV) * SENSOR_MAX_CM;
}
