// Deutsche Dokumentation
// Mini Web-GUI (Gateway): Implementierung
#include "webui.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LoRa.h>
#include "config.h"

// Vorwärtsdeklarationen lokaler Helfer
static String htmlEscape(const String &s);
static String fmtAge(unsigned long sinceMs);

// Zustandswerte (werden extern gesetzt/abgerufen – hier nur als Platzhalter deklariert)
extern String g_lastValue;     // letzter Wasserstand
extern unsigned long g_lastLoRaMs; // Zeitstempel letzte LoRa RX
extern int g_lastRssi;         // RSSI letzter RX

// OTA-AP Wunschzustand pro Sensor (unbestätigt)
struct OtaState { uint8_t sid; int8_t desired; unsigned long ts; };
static OtaState g_otaStates[4] = {{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0}};

static void setOtaDesired(uint8_t sid, bool on)
{
    for (auto &x : g_otaStates) {
        if (x.sid == sid || x.sid == 0) {
            if (x.sid == 0) x.sid = sid;
            x.desired = on?1:0; x.ts = millis();
            return;
        }
    }
    g_otaStates[0] = {sid, (int8_t)(on?1:0), millis()};
}

static int8_t getOtaDesired(uint8_t sid)
{
    for (auto &x : g_otaStates) if (x.sid == sid) return x.desired;
    return -1;
}

String buildStatusPageHtml()
{
    String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("-");
    String wifi = (WiFi.status() == WL_CONNECTED) ? String("verbunden") : String("--");
    String mqtt = String("siehe OLED/Log"); // Vereinfachte Darstellung – Details im Log
    String loraAge = g_lastLoRaMs ? fmtAge(millis()-g_lastLoRaMs) : String("--");

    String html;
    html.reserve(4000);
    html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>Drainage Gateway</title><style>");
    html += F("bodyfont-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,sans-serif;margin:0;background:#f6f7fb;color:#222}");
    html += F("headerdisplay:flex;align-items:center;gap:12px;padding:12px 16px;background:#1f2937;color:#fff}");
    html += F("mainpadding:16px;display:grid;grid-template-columns:1fr;gap:16px;max-width:900px;margin:0 auto}");
    html += F(".cardbackground:#fff;border:1px solid #e5e7eb;border-radius:10px;box-shadow:0 1px 2px rgba(0,0,0,.04)}");
    html += F(".card h2margin:0;padding:12px 16px;border-bottom:1px solid #eee;font-size:16px}");
    html += F(".card .bodypadding:12px 16px}");
    html += F("tableborder-collapse:collapse;width:100%} td,thborder:1px solid #e5e7eb;padding:8px;text-align:left}");
    html += F(".rowdisplay:flex;gap:16px;align-items:flex-start;flex-wrap:wrap}");
    html += F(".mutedcolor:#6b7280;font-size:12px}");
    html += F(".btndisplay:inline-block;margin-right:8px;padding:8px 10px;border-radius:8px;border:1px solid #d1d5db;background:#f9fafb}");
    html += F(".btn:hoverbackground:#f3f4f6}");
    html += F(".badgedisplay:inline-block;padding:2px 8px;border-radius:999px;background:#eef2ff;color:#3730a3;border:1px solid #c7d2fe;font-size:12px}");
    html += F("</style></head><body>");
    html += F("<header><div style='font-size:18px;font-weight:600'>Drainage Gateway</div><div class='muted'>LoRa · WLAN · MQTT</div></header>");
    html += F("<main>");

    html += F("<section class='card'><h2>Status</h2><div class='body'><div class='row'><div style='flex:1;min-width:260px'><table>");
    html += F("<tr><th>WLAN</th><td>"); html += htmlEscape(wifi); html += F("</td></tr>");
    html += F("<tr><th>IP</th><td>"); html += htmlEscape(ip); html += F("</td></tr>");
    html += F("<tr><th>MQTT</th><td>"); html += htmlEscape(mqtt); html += F("</td></tr>");
    html += F("<tr><th>LoRa letzte RX</th><td>"); html += htmlEscape(loraAge); html += F("</td></tr>");
    html += F("<tr><th>RSSI</th><td>"); html += String(g_lastRssi); html += F("</td></tr>");
    int8_t ota = getOtaDesired(1);
    html += F("<tr><th>Drainage WLAN-AP</th><td>");
    if (ota < 0) html += F("<span class='badge'>unbekannt</span>");
    else if (ota == 1) html += F("<span class='badge'>Eingeschaltet</span>");
    else html += F("<span class='badge'>Ausgeschaltet</span>");
    html += F("</td></tr>");
    html += F("</table></div></div></div></section>");

    html += F("<section class='card'><h2>Sensor OTA-AP steuern</h2><div class='body'>");
    html += F("<form method='POST' action='/sensor/ota'>");
    html += F("Sensor-ID: <input type='number' name='sid' min='1' max='255' value='1'>\n");
    html += F("<button class='btn' name='enable' value='1' type='submit'>AP einschalten</button>\n");
    html += F("<button class='btn' name='enable' value='0' type='submit'>AP ausschalten</button>\n");
    html += F("</form>");
    html += F("<div class='muted'>Hinweis: Status basiert auf dem zuletzt angeforderten Zustand (ohne Bestätigung).</div>");
    html += F("</div></section>");

    html += F("</main></body></html>");
    return html;
}

bool handleSensorOtaPost(int sid, bool enable)
{
    if (sid < 1 || sid > 255) return false;
    setOtaDesired((uint8_t)sid, enable);
    return true;
}

static String htmlEscape(const String &s)
{
    String o; o.reserve(s.length()+8);
    for (size_t i=0;i<s.length();++i){char c=s[i];
        if(c=='&') o+="&amp;"; else if(c=='<') o+="&lt;"; else if(c=='>') o+="&gt;"; else o+=c;}
    return o;
}

static String fmtAge(unsigned long sinceMs)
{
    unsigned long s = sinceMs / 1000UL;
    if (s < 60) return String(s) + "s";
    unsigned long m = s / 60UL; s %= 60UL;
    if (m < 60) return String(m) + "m" + (s?String(" ")+String(s)+"s":"");
    unsigned long h = m / 60UL; m %= 60UL;
    return String(h) + "h" + (m?String(" ")+String(m)+"m":"");
}
