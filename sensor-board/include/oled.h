#pragma once
// Deutsche Dokumentation
// OLED-Helfer für das Sensor-Board
#include <Arduino.h>

// Initialisiert das OLED (wenn in config.h aktiviert)
void oledInitSensor();

// Gibt zwei Zeilen Text aus (Projekt-Header + zwei Zeilen)
void oledPrint2Sensor(const String& line1, const String& line2);
