// Deutsche Dokumentation
// OLED-Implementierung (Sensor-Board, Heltec WiFi LoRa 32 V2)
#include "oled.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

static const int OLED_SDA = 4;
static const int OLED_SCL = 15;
static const uint8_t OLED_ADDR = 0x3C;
static const int OLED_RST = 16;
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
static bool g_oledOk = false;

void oledInitSensor()
{
    if (!OLED_ENABLED) return;
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(400000);
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D))
    {
        g_oledOk = true;
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("HELTEC OLED OK");
        display.display();
    }
}

void oledPrint2Sensor(const String& line1, const String& line2)
{
    if (!OLED_ENABLED || !g_oledOk) return;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Sensor-Board");
    display.setCursor(0, 12);
    display.println(line1);
    display.setCursor(0, 24);
    display.println(line2);
    display.display();
}
