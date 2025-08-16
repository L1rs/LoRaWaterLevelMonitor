# Entwicklungsauftrag: LoRa-basiertes Wasserstand-Messsystem mit MQTT-Anbindung

**Sprache:** Deutsch (Code-Kommentare, Readme, Dokumentation)  
**Framework:** PlatformIO (Visual Studio Code) mit Arduino-Framework  
**Projektziel:** Siehe unten – bitte ALLE Punkte umsetzen.  

---

## 1. Hardware
- 2× Heltec ESP32 LoRa V2 Boards (OLED, SX1276, ESP32)
- Analoger Wasserdrucksensor
  - Betrieb: 5V
  - Ausgang: 0V = 0 cm, 3,3V = 500 cm (5 m)
  - Messbereich: 0–5 m
- Kommunikation:
  - LoRa 868 MHz (EU)
  - WLAN am Gateway
  - MQTT mit Authentifizierung
- Zielsystem: Home Assistant mit MQTT Discovery, wenn einfach umsetzbar

---

## 2. Funktionsweise
### Sensor-Board (im Drainageschacht)
- ADC-Messung des Sensors
- Umrechnung: `Tiefe_cm = (ADC_Spannung / 3.3) * 500`
- Plausibilitätsprüfung (0–500 cm), sonst Fehlercode senden
- Übertragung per LoRa ans Gateway
- Intervall: 1× pro Minute
- OLED zeigt aktuellen Wert & Status
- Betrieb per USB-Netzteil (kein Deep Sleep zwingend nötig)

### Gateway-Board (im Haus)
- Empfang per LoRa
- WLAN-Verbindung
- MQTT-Senden:
home/drainage/waterlevel_cm
home/drainage/status

- MQTT-Auth (Benutzername & Passwort)
- Optional: MQTT Discovery
- OLED zeigt letzten Wert & Status

---

## 3. Technische Anforderungen
- Arduino-Framework in PlatformIO
- Bibliotheken:
- `heltec.h`
- `WiFi.h`
- `PubSubClient.h`
- LoRa-Einstellungen:
- Frequenz: 868 MHz
- Standard-SF/BW/CR, falls nicht anders nötig
- Konfigurationsdatei (`config.h`) mit allen zentralen Parametern:
- MQTT-Zugangsdaten
- LoRa-Parameter
- Messintervall
- Pinbelegung
- Fehlerbehandlung:
- LoRa- oder WLAN-Ausfall → Reconnect
- Unplausible Werte → Fehlercode senden

---

## 4. Struktur
/drainage-lora-project
/sensor-board
platformio.ini
/src/main.cpp
/include/config.h
/gateway-board
platformio.ini
/src/main.cpp
/include/config.h
README.md
LICENSE

---

## 5. Dokumentation
- Vollständige README.md:
  - Projektübersicht
  - Hardware-Aufbau
  - Installationsanleitung
  - Konfiguration
  - Home Assistant-Integration
  - Beispiel-MQTT-Ausgaben
- Kommentare im Code auf Deutsch
- GitHub-taugliche Struktur
- Lizenzdatei (MIT oder andere freie Lizenz)

---

## 6. Aufgabe für die KI
> **Bitte alle oben genannten Punkte vollständig umsetzen**.  
> Erstelle vollständigen, lauffähigen Code für beide Boards.  
> Fülle `main.cpp` und `config.h` in beiden Projekten.  
> Erstelle eine ausführliche README.md.  
> Nutze deutsche Kommentare.  
> Dokumentiere so, dass das Projekt sofort getestet und in GitHub veröffentlicht werden kann.