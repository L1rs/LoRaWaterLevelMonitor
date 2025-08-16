// Deutsche Dokumentation
// OTA-AP Implementierung (Sensor-Board)
#include "ota_ap.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "oled.h"

static bool s_active = false;

static String macSuffix()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[13];
    snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    return String(buf);
}

void otaApInit()
{
    if (!OTA_AP_ENABLED || s_active) return;
    WiFi.mode(WIFI_AP);
    String ssid = String(OTA_AP_SSID_PREFIX) + macSuffix();
    WiFi.softAP(ssid.c_str(), OTA_AP_PASSWORD);

    ArduinoOTA.setHostname(ssid.c_str());
    ArduinoOTA.setPassword(OTA_AP_PASSWORD);

    ArduinoOTA.onStart([](){
        Serial.println("OTA Start (Sensor AP)");
    });
    ArduinoOTA.onEnd([](){
        Serial.println("\nOTA Ende");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){
        Serial.printf("OTA Fortschritt: %u%%\r", (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error){
        Serial.printf("\nOTA Fehler[%u]\n", error);
    });

    ArduinoOTA.begin();
    s_active = true;

    Serial.printf("AP SSID: %s\n", ssid.c_str());
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("OTA bereit (Sensor)");
    oledPrint2Sensor(String("AP: ") + ssid, String("IP: ") + WiFi.softAPIP().toString());
}

void otaApLoop()
{
    if (s_active) ArduinoOTA.handle();
}

void otaApStop()
{
    if (!s_active) return;
    WiFi.softAPdisconnect(true);
    s_active = false;
    Serial.println("OTA AP gestoppt");
}
