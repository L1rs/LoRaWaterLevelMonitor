#pragma once
// Deutsche Dokumentation
// Sensor-Messmodul: Mittelung in mV und Umrechnung mV -> cm
#include <stdint.h>

// Liest gemittelte Millivolt vom angegebenen ADC-Pin (ESP32 ADC1 bevorzugt)
uint32_t readMilliVoltsAveraged(int pin, int samples = 32);

// Rechnet mV in Zentimeter Wassertiefe um (Parameter aus config.h)
float mvToDepthCm(uint32_t mv);
