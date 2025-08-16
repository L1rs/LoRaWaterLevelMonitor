# LoRaWaterLevelMonitor 🌊📡

LoRa-basiertes Wasserstand-Messsystem für Drainageschächte auf Basis von zwei ESP32-Board's vom Typ "Heltec WiFi LoRa 32 (V2)". Misst den Wasserstand cm-Genau via Analogem-Wasserdrucksensor und überträgt die Daten per LoRa (868 MHz) an ein Gateway, das sie via MQTT z.B. an HomeAssistant bereitstellt.

## Inhaltsverzeichnis
- [Features](#features)
- [Hardware](#hardware)
- [Projektstruktur](#projektstruktur)
- [Installation](#installation)
- [Konfiguration](#konfiguration)
- [Build & Upload](#build--upload)
- [OTA-Updates](#ota-updates)
- [Web-GUI](#web-gui)
- [MQTT-Integration](#mqtt-integration)
- [Entwicklung](#entwicklung)
- [Lizenz](#lizenz)

## Features ✨
- **LoRa-Kommunikation** (868 MHz, EU)
- **Präzise Messung** via analogem Drucksensor mit Kalibrierung
- **Sichere Übertragung** mit AES-128-CTR + HMAC-SHA256
- **Modularer Aufbau** (getrennte Projekte für Sensor und Gateway)
- **Web-GUI** für Statusüberwachung
- **Home Assistant** Integration via MQTT Discovery
- **OTA-Updates** für beide Boards

## Hardware 🛠️
### Notwendige Komponenten
| Komponente | Empfohlenes Modell | Beschaffung* |
|------------|--------------------|--------------|
| **LoRa-Boards** | 2x Heltec WiFi LoRa 32 (V2) | [Aliexpress](https://de.aliexpress.com/item/1005005967763162.html) |
| **Drucksensor** | Tauchwasserstandsensor "0-3.3V out 5VPower" (Messbereich 0-5m) | [Aliexpress](https://de.aliexpress.com/item/1005005928682318.html) |
| **Netzteil** | USB-Netzteil, 5V, 2.1A | [Aliexpress](https://de.aliexpress.com/item/1005008831409743.html) |

### Sensor-Board (im Schacht)
- **Heltec WiFi LoRa 32 (V2)**
  - ESP32 + SX1276 LoRa
  - Integriertes OLED (optional nutzbar)
- **Analoger Drucksensor**
  - Messbereich: 0-5m Wasserstand
  - Analoger Ausgang: 0-3.3V
  - Spannungsversprgung: 5V
- **Verdrahtung**:
  ```plaintext
  Sensor VCC → 5V
  Sensor GND → GND
  Sensor OUT → GPIO36 (ADC1_CH0)
  ```

### Gateway-Board (im Haus)
- **Heltec WiFi LoRa 32 (V2)**
  - Gleiches Board wie Sensor für Kompatibilität
  - WLAN für MQTT-Verbindung
- **Externe Antenne** empfohlen für bessere Reichweite

## Projektstruktur 📂
```
/LoRaWaterLevelMonitor
├── sensor-board/              # Sensor-Firmware
│   ├── src/                   # Modularisierter Quellcode
│   ├── include/               # Header (config.h lokal)
│   └── platformio.ini
├── gateway-board/             # Gateway-Firmware
│   ├── src/                   # Modularisierter Quellcode
│   ├── include/               # Header (config.h lokal)
│   └── platformio.ini
├── common/                    # Gemeinsame Module
│   ├── crypto.{h,cpp}         # Verschlüsselung
│   └── util.{h,cpp}           # Hilfsfunktionen
├── README.md
└── LICENSE
```

## Installation 💻
1. Repository klonen:
   ```bash
   git clone https://github.com/IhrBenutzername/LoRaWaterLevelMonitor.git
   cd LoRaWaterLevelMonitor
   ```

2. Konfiguration vorbereiten:
   ```bash
   cp sensor-board/include/config.h.example sensor-board/include/config.h
   cp gateway-board/include/config.h.example gateway-board/include/config.h
   ```

3. PlatformIO-Projekt in VS Code öffnen

## Konfiguration ⚙️

### 🔄 Wichtiger Hinweis zu Konfigurationsdateien
Die Dateien `config.h` enthalten sensible Zugangsdaten (WLAN-Passwörter, MQTT-Credentials) und werden **nicht** in das Git-Repository eingecheckt. Stattdessen werden Vorlagen (`config.h.example`) bereitgestellt.

**So gehst du vor:**
1. Kopiere `config.h.example` zu `config.h` in beiden Board-Verzeichnissen
2. Passe die Werte in den `config.h` Dateien an deine Umgebung an
3. Die `config.h` Dateien bleiben lokal und werden nicht mit GitHub synchronisiert

### Sensor-Board (`sensor-board/include/config.h`)
```cpp
// LoRa
static const long LORA_FREQUENCY_HZ = 868E6; // 868 MHz

// Sensor-Kalibrierung
static const uint32_t SENSOR_VREF_MV = 3300; // mV bei Maximalwert
static const float SENSOR_MV_SCALE = 0.872f; // Kalibrierfaktor

// OTA-AP
static const char *OTA_AP_PASSWORD = "IHR_OTA_PASSWORT"; // Mind. 8 Zeichen

// Verschlüsselungs-Schlüssel (ÄNDERN!)
static const uint8_t AES_KEY[16]  = { /* 16 Bytes Schlüssel */ };
static const uint8_t HMAC_KEY[16] = { /* 16 Bytes Schlüssel */ };
```

### Gateway-Board (`gateway-board/include/config.h`)
```cpp
// WLAN
static const char *WIFI_SSID = "IHR_WLAN";
static const char *WIFI_PASSWORD = "IHR_WLAN_PASSWORT";

// MQTT
static const char *MQTT_HOST = "192.168.1.100";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_USER = "mqtt_user";
static const char *MQTT_PASS = "mqtt_passwort";

// OTA
static const char *OTA_PASSWORD = "IHR_OTA_PASSWORT";

// Verschlüsselungs-Schlüssel (identisch zum Sensor!)
static const uint8_t AES_KEY[16]  = { /* 16 Bytes Schlüssel */ };
static const uint8_t HMAC_KEY[16] = { /* 16 Bytes Schlüssel */ };
```

**⚠️ Wichtig:** Verwende niemals die Beispielschlüssel in Produktion!

## Build & Upload ⚡
### Sensor-Board
```bash
pio run -e heltec_wifi_lora_32_V2 -t upload
```

### Gateway-Board
```bash
pio run -e heltec_wifi_lora_32_V2 -t upload
```

### Gateway-Board (OTA)
```bash
pio run -e heltec_wifi_lora_32_V2 -t upload --upload-port drainage-gw-xxxxxxxx.local
```

## OTA-Updates 🔄

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

## Web-GUI 🌐

Das Gateway-Board stellt eine kleine Web-GUI zur Verfügung:

**URL:** `http://<gateway-ip>/`

**Features:**
- Statusübersicht (WLAN, MQTT, LoRa)
- Letzte Messwerte und Empfangszeit
- RSSI-Anzeige
- OTA-AP-Steuerung für das Sensor-Board

## MQTT-Integration 📡

### Topics
- `lora/drainage/waterlevel_cm` – Messwert in Zentimetern (z. B. `123.4`)
- `lora/drainage/rssi` – RSSI des zuletzt empfangenen LoRa-Pakets (Integer)

### Beispielausgaben
```
lora/drainage/waterlevel_cm => 123.4
lora/drainage/rssi          => -112
```
### Home Assistant (optional)
Bei aktivierter Discovery erscheint die Entität automatisch:
- Gerät: `Drainage Gateway`
- Sensor: `Drainage Wasserstand` (Einheit: cm)
- Discovery-Topic: `<homeassistant>/sensor/<HA_NODE_ID>/waterlevel/config`

Aktivierung über `ENABLE_HA_DISCOVERY = true` in der Gateway-Konfiguration.

## Nutzung 🚀

### Sensor-Board (im Drainageschacht)
- Misst über ADC den Drucksensor
- Rechnet um: `Tiefe_cm = (ADC_Spannung / 3.3) * 500`
- Prüft Plausibilität (0–500 cm), sendet Status `OK` oder `ERR`
- Sendet jede Minute ein LoRa-Paket: `WATER_CM:<wert>;STATUS:<OK|ERR>;MID:<id>`
- OLED zeigt aktuellen Wert und Status

### Gateway-Board (im Haus)
- Empfängt LoRa-Pakete
- Verbindet sich mit dem WLAN und MQTT-Broker (mit Authentifizierung)
- Veröffentlicht den Messwert per MQTT
- Optional: Veröffentlicht Home Assistant Discovery-Config bei MQTT-Verbindung
- OLED zeigt letzten Wert und Status

## Fehlersuche 🛠️
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

## Entwicklung 👨‍💻

### Code-Standards
- Deutsche Kommentare
- Modulare Struktur
- Keine Secrets im versionierten Code
- Ausführliche Dokumentation

### Beitrag leisten
Pull Requests sind willkommen!

**Entwicklungsschritte:**
1. Fork des Repositories erstellen
2. Feature-Branch erstellen
3. Änderungen committen
4. Pull Request erstellen

**Issues:**
- Bugs und Feature-Requests über GitHub Issues melden
- Template verwenden und detaillierte Informationen angeben

## Support & Community 📞

**Probleme melden:** [GitHub Issues](https://github.com/IhrBenutzername/LoRaWaterLevelMonitor/issues)

**Fragen & Diskussion:** Nutze die [Discussions](https://github.com/IhrBenutzername/LoRaWaterLevelMonitor/discussions)

---

**⭐ Wenn dir dieses Projekt geholfen hat, gib ihm einen Stern auf GitHub!**

## Lizenz 📜

Dieses Projekt steht unter der MIT-Lizenz. Siehe [LICENSE](LICENSE) für Details.

