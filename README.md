## 🔄 Over-the-Air (OTA) Updates

Beide Boards unterstützen OTA-Updates für einfache Firmware-Aktualisierung ohne physischen Zugang.

### Gateway-Board (WLAN-basiert)
Das Gateway stellt nach erfolgreicher WLAN-Verbindung einen OTA-Service bereit:

**Konfiguration:**
- `OTA_ENABLED = true` in `gateway-board/include/config.h`
- `OTA_PASSWORD` auf sicheres Passwort setzen
- Hostname: `drainage-gw-<MAC-Adresse>.local`

**Nutzung:**
```bash
# Über PlatformIO CLI
pio run -e heltec_wifi_lora_32_V2 -t upload --upload-port drainage-gw-xxxxxxxx.local

# Hostname aus Serial Monitor ablesen oder in Router/mDNS-Browser finden
```

### Sensor-Board (Access Point-basiert)
Da das Sensor-Board im Schacht keinen WLAN-Zugang hat, erstellt es einen eigenen Access Point:

**Konfiguration:**
- `OTA_AP_ENABLED = true` in `sensor-board/include/config.h`
- `OTA_AP_PASSWORD` auf sicheres Passwort setzen
- SSID: `drainage-sensor-<MAC-Adresse>`

**Nutzung:**
1. Mit Laptop/Smartphone in das Sensor-AP einwählen
2. OTA-Update über PlatformIO durchführen
3. Nach Update trennt sich der AP automatisch

# LoRaWaterLevelMonitor

*LoRa-basiertes Wasserstand-Messsystem für Drainagesysteme*

## Übersicht
Dieses Projekt misst den Wasserstand in einem Drainageschacht mit einem analogen Drucksensor und überträgt die Messwerte über LoRa (868 MHz) an ein Gateway. Das Gateway verbindet sich per WLAN mit einem MQTT-Broker und veröffentlicht die Daten. Optional wird Home Assistant via MQTT Discovery automatisch eingebunden.

## Hardware
- 2× Heltec ESP32 LoRa V2 Boards (OLED, SX1276, ESP32)
- Analoger Wasserdrucksensor
  - Betrieb: 5V
  - Ausgang: 0V = 0 cm, 3,3V = 500 cm (5 m)
  - Messbereich: 0–5 m
- Kommunikation
  - LoRa 868 MHz (EU)
  - WLAN am Gateway
  - MQTT mit Authentifizierung

### Verdrahtung (Sensor-Board)
- Sensor-Ausgang an `GPIO36` (ADC1_CH0) des Heltec-Boards
- Sensor-GND an GND, Sensor-Vcc an 5V-Versorgung
- Hinweis: Bei langen Leitungen und 5V-Signal ggf. Spannungsteiler/Analog-Front-End vorsehen, um 3.3V am ADC nicht zu überschreiten.

## Installation
Voraussetzung: Visual Studio Code mit PlatformIO-Erweiterung.

### Projektstruktur
```
/LoRaWaterLevelMonitor
├── sensor-board/
│   ├── platformio.ini
│   ├── src/main.cpp
│   └── include/
│       ├── config.h.example        # Vorlage für Konfiguration
│       └── config.h                # Lokale Konfiguration (nicht versioniert)
├── gateway-board/
│   ├── platformio.ini
│   ├── src/main.cpp
│   └── include/
│       ├── config.h.example        # Vorlage für Konfiguration
│       └── config.h                # Lokale Konfiguration (nicht versioniert)
├── README.md
├── LICENSE
└── .gitignore
```

### Installationsschritte
1. **Repository klonen:**
   ```bash
   git clone https://github.com/L1rs/LoRaWaterLevelMonitor.git
   cd LoRaWaterLevelMonitor
   ```

2. **Projekt in VS Code öffnen:**
   ```bash
   code .
   ```

3. **Konfigurationsdateien erstellen:**
   ```bash
   # Sensor-Board Konfiguration
   cp sensor-board/include/config.h.example sensor-board/include/config.h
   
   # Gateway-Board Konfiguration  
   cp gateway-board/include/config.h.example gateway-board/include/config.h
   ```

4. **Konfiguration anpassen** (siehe Abschnitt "Konfiguration")

5. **Projekte bauen und flashen:**
   - Im PlatformIO-Panel das passende Environment auswählen
   - "Build" und "Upload" für beide Boards durchführen

## Konfiguration

### 🔄 Wichtiger Hinweis zu Konfigurationsdateien
Die Dateien `config.h` enthalten sensible Zugangsdaten (WLAN-Passwörter, MQTT-Credentials) und werden **nicht** in das Git-Repository eingecheckt. Stattdessen werden Vorlagen (`config.h.example`) bereitgestellt.

**So gehst du vor:**
1. Kopiere `config.h.example` zu `config.h` in beiden Board-Verzeichnissen
2. Passe die Werte in den `config.h` Dateien an deine Umgebung an
3. Die `config.h` Dateien bleiben lokal und werden nicht mit GitHub synchronisiert

### Gateway-Board Konfiguration (`gateway-board/include/config.h`)
**WLAN-Einstellungen:**
```cpp
static const char *WIFI_SSID = "DEIN_WLAN_NAME";           // ÄNDERN!
static const char *WIFI_PASSWORD = "DEIN_WLAN_PASSWORT";   // ÄNDERN!
```

**MQTT-Broker-Einstellungen:**
```cpp
static const char *MQTT_HOST = "192.168.1.100";  // IP deines MQTT-Brokers
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_USER = "dein_mqtt_benutzer";      // ÄNDERN!
static const char *MQTT_PASS = "dein_mqtt_passwort";      // ÄNDERN!
```

**OTA-Update-Passwort:**
```cpp
static const char *OTA_PASSWORD = "DEIN_OTA_PASSWORT";     // ÄNDERN!
```

### Sensor-Board Konfiguration (`sensor-board/include/config.h`)
**OTA-Access-Point-Passwort:**
```cpp
static const char *OTA_AP_PASSWORD = "HIER_DEIN_OTA_PASSWORT";  // ÄNDERN!
```

**Verschlüsselungs-Schlüssel:**
Für beide Boards müssen identische AES- und HMAC-Schlüssel konfiguriert werden:
```cpp
static const uint8_t AES_KEY[16]  = { /* 16 Bytes Schlüssel */ };  // ÄNDERN!
static const uint8_t HMAC_KEY[16] = { /* 16 Bytes Schlüssel */ }; // ÄNDERN!
```
**⚠️ Wichtig:** Verwende niemals die Beispielschlüssel in Produktion!

### Weitere Parameter
**LoRa:**
- Frequenz: 868 MHz (EU-Band)
- Bei Bedarf Spreading Factor/Bandwidth in Code anpassen

**Sensor-Board:**
- `SENSOR_ADC_PIN`: GPIO36 (Standard)
- `MEASURE_INTERVAL_MS`: 60.000 ms (1 Minute)
- Messbereich: 0–500 cm Wassertiefe

**MQTT-Topics:**
- `lora/drainage/waterlevel_cm` – Wasserstand in cm
- `lora/drainage/rssi` – Signalstärke des letzten Pakets

**Home Assistant Discovery:** Über `ENABLE_HA_DISCOVERY` aktivierbar

## Nutzung
### Sensor-Board (im Drainageschacht)
- Misst über ADC den Drucksensor.
- Rechnet um: `Tiefe_cm = (ADC_Spannung / 3.3) * 500`.
- Prüft Plausibilität (0–500 cm), sendet Status `OK` oder `ERR`.
- Sendet jede Minute ein LoRa-Paket: `WATER_CM:<wert>;STATUS:<OK|ERR>;MID:<id>`.
- OLED zeigt aktuellen Wert und Status.

### Gateway-Board (im Haus)
- Empfängt LoRa-Pakete.
- Verbindet sich mit dem WLAN und MQTT-Broker (mit Authentifizierung).
- Veröffentlicht den Messwert per MQTT.
- Optional: Veröffentlicht Home Assistant Discovery-Config bei MQTT-Verbindung.
- OLED zeigt letzten Wert und Status.

## MQTT Topics
- `lora/drainage/waterlevel_cm` – Messwert in Zentimetern (z. B. `123.4`)
- `lora/drainage/rssi` – RSSI des zuletzt empfangenen LoRa-Pakets (Integer)

### Beispielausgaben
```
lora/drainage/waterlevel_cm => 123.4
lora/drainage/rssi          => -112
```

Hinweis: Es wird kein dediziertes Status-/LWT-Topic mehr verwendet.

### Home Assistant (optional)
Bei aktivierter Discovery erscheint die Entität automatisch:
- Gerät: `Drainage Gateway`
- Sensor: `Drainage Wasserstand` (Einheit: cm)
Discovery-Topic: `<homeassistant>/sensor/<HA_NODE_ID>/waterlevel/config`

## 📋 Lizenz
Dieses Projekt steht unter der MIT-Lizenz. Siehe [`LICENSE`](LICENSE) für Details.

## 📞 Support & Community

**Probleme melden:** [GitHub Issues](https://github.com/DEIN-USERNAME/LoRaWaterLevelMonitor/issues)

**Fragen & Diskussion:** Nutze die [Discussions](https://github.com/DEIN-USERNAME/LoRaWaterLevelMonitor/discussions)

**Beitragen:** Pull Requests sind willkommen! Siehe Abschnitt "Entwicklung & Beitrag"

---

**⭐ Wenn dir dieses Projekt geholfen hat, gib ihm einen Stern auf GitHub!**

## 🚀 Erste Schritte nach der Installation

1. **Konfiguration prüfen:**
   - Sind alle Zugangsdaten korrekt in den `config.h` Dateien eingetragen?
   - Sind die Verschlüsselungsschlüssel in beiden Boards identisch?

2. **Gateway testen:**
   - Gateway-Board flashen und Serial Monitor öffnen
   - WLAN-Verbindung sollte erfolgreich sein
   - MQTT-Verbindung sollte hergestellt werden

3. **Sensor testen:**
   - Sensor-Board flashen und Serial Monitor öffnen
   - Messwerte sollten plausibel sein (0-500 cm)
   - LoRa-Pakete werden gesendet

4. **Kommunikation prüfen:**
   - Gateway sollte Pakete vom Sensor empfangen
   - MQTT-Broker sollte die Nachrichten erhalten
   - Bei Home Assistant: Entitäten sollten automatisch erscheinen

## 🛠️ Fehlersuche

**OLED Display Probleme:**
- Prüfen, ob Heltec-Bibliothek korrekt eingebunden ist
- OLED kann über `OLED_ENABLED = false` deaktiviert werden

**LoRa Kommunikationsprobleme:**
- Frequenz (`868E6`) in beiden Projekten identisch?
- Antennen korrekt montiert und angeschlossen?
- Reichweite: Im Gebäude ca. 100-200m, im Freien mehrere km

**WLAN/MQTT Verbindungsprobleme:**
- SSID und Passwort korrekt?
- MQTT-Broker erreichbar? (ping testen)
- MQTT-Benutzerdaten korrekt?
- Firewall/Router-Einstellungen prüfen

**Home Assistant Integration:**
- MQTT-Integration in HA aktiv?
- Discovery-Präfix (`homeassistant`) korrekt?
- HA-Logs auf Fehler prüfen

**Verschlüsselungsprobleme:**
- Sind AES_KEY und HMAC_KEY in beiden Boards identisch?
- Ist `ENCRYPTION_ENABLED` in beiden Boards auf `true`?
- Sensor-ID in der Whitelist des Gateways?

## 📝 Entwicklung & Beitrag

Dieses Projekt wurde mit PlatformIO entwickelt. Für Entwickler:

**Code-Standards:**
- Kommentare auf Deutsch
- Ausführliche Dokumentation
- Keine Secrets im Code (nur in lokalen `config.h`)

**Pull Requests:**
- Fork des Repositories erstellen
- Feature-Branch erstellen
- Änderungen committen
- Pull Request erstellen

**Issues:**
- Bugs und Feature-Requests über GitHub Issues melden
- Template verwenden und detaillierte Informationen angeben
