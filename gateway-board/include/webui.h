#pragma once
// Deutsche Dokumentation
// Mini Web-GUI (Gateway): Statusseite und einfache Aktionen
#include <WString.h>

// Erzeugt HTML für die Statusseite
String buildStatusPageHtml();

// Verarbeitet POST /sensor/ota
// Rückgabe: true = gültig verarbeitet
bool handleSensorOtaPost(int sid, bool enable);
