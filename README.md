# LoRaWaterLevelMonitor

Ein DIY-Projekt zur Messung des Wasserstands in einem Drainageschacht mit Hilfe von **LoRa**, **MQTT** und **Home Assistant**.  
Das Projekt besteht aus zwei ESP32-Boards (Heltec ESP32 LoRa V2), die drahtlos per LoRa kommunizieren.  
Alle Code-Kommentare und Dokumentationen sind auf **Deutsch** gehalten.

---

## Inhalt

1. [Überblick](#überblick)  
2. [Hardware](#hardware)  
3. [Einkaufsliste](#einkaufsliste)  
4. [Funktionsweise](#funktionsweise)  
5. [Projektstruktur](#projektstruktur)  
6. [Installation & Konfiguration](#installation--konfiguration)  
7. [Over-the-Air & Zusatzfunktionen](#over-the-air--zusatzfunktionen)  
8. [Web-UI & Access-Point](#web-ui--access-point)  
9. [Grafiken & Visualisierungen](#grafiken--visualisierungen)  
10. [Home Assistant Integration](#home-assistant-integration)  
11. [Mitmachen & Feedback](#mitmachen--feedback)  
12. [Lizenz](#lizenz)

---

## Überblick

Dieses Projekt besteht aus zwei Heltec ESP32 LoRa V2 Boards:

- **Sensor-Board** im Drainageschacht:  
  Liest den Analogdrucksensor, rechnet Spannung in cm Wasserstand und sendet via LoRa.
- **Gateway-Board** im Haus:  
  Empfängt die Daten, verbindet sich mit WLAN, sendet die Werte per MQTT an Home Assistant.  

Beide Boards verfügen über ein OLED-Display zur Anzeige von Status & Messwerten.  
Das Gateway bietet zusätzlich OTA-Updates (Over the Air), Display-Steuerung per Tastendruck und eine Web-UI.

---

## Hardware

- **Heltec ESP32 LoRa V2** (2 Stück, jeweils mit OLED und SX1276 LoRa Modul)  
- **Analoger Wasserdrucksensor**  
  - Betrieb: 5 V (USB-Netzteil)  
  - Ausgang: 0 V ≙ 0 cm, 3,3 V ≙ 500 cm (5 m)  
- **Kommunikation:** LoRa (868 MHz, EU), MQTT mit Authentifizierung zum lokalen Broker  
- **Zielplattform:** Home Assistant mit MQTT Discovery (optional)

---

## Einkaufsliste

Hier eine Liste mit den wichtigsten Bauteilen und Links (Beispielquellen, bitte nach Verfügbarkeit suchen):

- [Heltec ESP32 LoRa V2 Board](https://heltec.org/project/wifi-lora-32/) ×2  
- [Analoger Wasserdrucksensor 0–5 m, 0–3,3 V Ausgang](https://www.amazon.de/s?k=water+pressure+sensor+0-5m)  
- [USB-Netzteile 5 V](https://www.amazon.de/s?k=usb+netzteil+5v)  
- [Micro-USB Kabel](https://www.amazon.de/s?k=micro+usb+kabel)  
- Optional: [Gehäuse für ESP32 Boards](https://www.thingiverse.com/search?q=esp32+case) (3D-Druck)  

---

## Funktionsweise

1. **Sensor-Board** misst jede Minute den Wasserstand (0–500 cm).  
2. Werte werden per **LoRa (868 MHz)** an das Gateway gesendet.  
3. **Gateway-Board** empfängt die Daten, stellt WLAN-Verbindung her und sendet per **MQTT** an Home Assistant.  
4. Beide Boards zeigen Werte und Statusmeldungen auf ihrem OLED an.  

---

## Projektstruktur

```text
/LoraWaterLevelMonitor
│
├── PROJECT_TASK.md
├── README.md
├── LICENSE
├── .gitignore
├── sensor-board
│   ├── platformio.ini
│   ├── src/main.cpp
│   └── include/config.h.example
└── gateway-board
    ├── platformio.ini
    ├── src/main.cpp
    └── include/config.h.example
```

---

## Installation & Konfiguration

### 1. Repository klonen
```bash
git clone https://github.com/L1rs/LoRaWaterLevelMonitor.git
cd LoRaWaterLevelMonitor
```

### 2. Konfigurationsdateien anpassen
- In beiden Ordnern `sensor-board/include/` und `gateway-board/include/` die Datei  
  `config.h.example` → **kopieren und umbenennen** in `config.h`  
- Trage dort deine persönlichen Daten ein:
  - WLAN SSID & Passwort  
  - MQTT-Server IP, Port, Benutzername, Passwort  
  - Messintervall (Standard: 60 Sekunden)

### 3. Projekt kompilieren & flashen
Mit [PlatformIO](https://platformio.org/) (Visual Studio Code Plugin):

```bash
cd sensor-board
pio run --target upload

cd ../gateway-board
pio run --target upload
```

### 4. Seriellen Monitor öffnen (zum Debuggen)
```bash
pio device monitor
```

### 5. OTA-Updates nutzen
Nach dem ersten Flashen via USB kannst du künftige Updates **drahtlos (Over-the-Air)** einspielen:

```bash
pio run --target upload --upload-port <ESP32-IP-Adresse>
```

---

## Over-the-Air & Zusatzfunktionen

- **OTA-Updates:** Beide Boards unterstützen Over-the-Air Updates nach der Erstinstallation.  
- **Gateway Display Steuerung:**  
  - Kurzer Tastendruck → Display an/aus schalten  
  - Spart Strom und verlängert die OLED-Lebensdauer  

---

## Web-UI & Access-Point

- **Gateway-Board Web-UI:**  
  Über die IP-Adresse des Gateway-Boards im Browser erreichbar.  
  Zeigt Statusinformationen und letzte Messwerte an.  
  Außerdem kann man hier das WLAN-Access-Point-Feature starten.

- **Sensor-Board WLAN Access-Point:**  
  Kann als eigener WLAN-Access-Point gestartet werden.  
  Darüber ist es möglich, **OTA-Updates** auch ohne bestehendes Heimnetzwerk durchzuführen.  
  → Praktisch, wenn das Sensor-Board im Schacht schwer erreichbar ist.

---

## Grafiken & Visualisierungen

### ASCII-Blockdiagramm (Systemübersicht)

```text
[ Wasserdrucksensor ] --(analog)--> [ Sensor-Board (ESP32 + LoRa) ]
                                          |
                                      (LoRa 868 MHz)
                                          |
                                          v
                              [ Gateway-Board (ESP32 + LoRa) ]
                                          |
                        +-----------------+------------------+
                        |                                    |
                   (WLAN + MQTT)                        (Web-UI)
                        |                                    |
                        v                                    v
            [ Home Assistant / MQTT Broker ]        [ Browser / PC / Handy ]
```

### Weitere sinnvolle Grafiken (noch zu erstellen)
- **Blockdiagramm** mit schöner Grafik (z. B. in diagrams.net)  
- **Verdrahtungsplan** (Sensor → Heltec ADC-Pin), z. B. mit Fritzing  
- **OLED-Screenshots** (Sensor-Board zeigt Wasserstand, Gateway zeigt Status)  
- **Home Assistant Dashboard-Screenshot** mit integriertem Sensor  

---

## Home Assistant Integration

- MQTT Topics:
  - `home/drainage/waterlevel_cm` → aktueller Wasserstand in cm  
  - `home/drainage/status` → Status- oder Fehlercode  
- Optional: **MQTT Discovery** aktivieren → Gerät wird automatisch erkannt.  

---

## Mitmachen & Feedback

- Issues & Pull Requests willkommen!  
- Verbesserungsvorschläge oder neue Features sind ausdrücklich erwünscht.  

---

## Lizenz

Dieses Projekt ist unter der [MIT-Lizenz](LICENSE) veröffentlicht.  
Du darfst es nutzen, modifizieren und weitergeben — auch kommerziell.