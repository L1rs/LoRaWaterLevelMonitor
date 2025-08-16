#pragma once
// Deutsche Dokumentation
// OTA im Access-Point Modus (Sensor-Board)

// Startet den SoftAP + ArduinoOTA Service (gemäß config.h)
void otaApInit();

// Muss regelmäßig im loop() aufgerufen werden, um OTA zu bedienen
void otaApLoop();

// Stoppt den SoftAP und den OTA-Service
void otaApStop();
