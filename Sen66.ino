#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <ArduinoOTA.h>
#include <SensirionI2cSen66.h>
#include <ESPmDNS.h>

#ifndef NO_ERROR
#define NO_ERROR 0
#endif

#define TFT_DC 9
#define TFT_CS 10
#define TFT_RST 8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

void logBoth(const String& msg) {
  Serial.println(msg);
}

static const int I2C_SDA = 2;
static const int I2C_SCL = 3;

SensirionI2cSen66 sensor;
static int16_t error = NO_ERROR;

static void printError(const char* what, int16_t err) {
  Serial.print("ERROR ");
  Serial.print(what);
  Serial.print(" code=");
  Serial.println(err);
}

static int scanFirstI2cAddress() {
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t e = Wire.endTransmission(true);
    if (e == 0) return addr;
    delay(2);
  }
  return -1;
}

static void i2cScanPrint() {
  Serial.println("\nI2C scan...");
  int found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t e = Wire.endTransmission(true);
    if (e == 0) {
      Serial.print("Found: 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
    delay(2);
  }
  Serial.print("Devices found: ");
  Serial.println(found);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  SPI.begin(6, -1, 7, 10);   // SCK, MISO, MOSI, CS
  tft.begin();
  tft.fillScreen(ILI9341_RED);
  tft.setTextColor(ILI9341_WHITE, ILI9341_RED); // Farbe
  tft.setTextSize(2); 

  //connectWiFi();

  logBoth("\n=== ESP32-C5 + SEN66 ===");

  // I2C on GPIO2/3
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // 100kHz, stabiler bei langen Kabeln
  logBoth("Wire started on SDA=2 SCL=3");

  i2cScanPrint();

  int addr = scanFirstI2cAddress();
  if (addr < 0) {
    logBoth("No I2C device found. Check 5V+GND, SDA/SCL, pullups.");
    return;
  }

  logBoth("Using I2C address: 0x");
  if (addr < 16) Serial.print('0');

  sensor.begin(Wire, (uint8_t)addr);

  error = sensor.deviceReset();
  if (error != NO_ERROR) { printError("deviceReset()", error); return; }
  delay(1200);

  error = sensor.startContinuousMeasurement();
  if (error != NO_ERROR) { printError("startContinuousMeasurement()", error); return; }

  logBoth("Measurement started.");
  logBoth("Per Telnet verbinden zu: " + WiFi.localIP().toString() + ":23");
}

void loop() {
  tft.fillScreen(ILI9341_RED);

  tft.setCursor(10, 10);           // Position             // Größe
  tft.println("Co2: 500ppm");

  float pm1p0=0, pm2p5=0, pm4p0=0, pm10p0=0;
  float rh=0, t=0, voc=0, nox=0;
  uint16_t co2=0;

  error = sensor.readMeasuredValues(pm1p0, pm2p5, pm4p0, pm10p0, rh, t, voc, nox, co2);
  if (error != NO_ERROR) {
    logBoth("Fehler: readMeasuredValues() code=" + String(error));
  } else {
    tft.setCursor(10, 10);           // Position             // Größe
    tft.println("Co2: " + String(co2) + "ppm");
    tft.setCursor(10, 30);           // Position
    tft.println("Temp: " + String(t, 1) + "grad");
    tft.setCursor(10, 50);  
    tft.println("Feuchtigkeit: " + String(rh, 1) + "%");
    tft.setCursor(10, 70);  
    tft.println("PM1.0: " + String(pm1p0, 1) + "ng/m3");
    tft.setCursor(10, 90);  
    tft.println("PM1.0: " + String(pm2p5, 1) + "ng/m3");
    tft.setCursor(10, 110);  
    tft.println("PM1.0: " + String(pm4p0, 1) + "ng/m3");
    tft.setCursor(10, 130);  
    tft.println("PM1.0: " + String(pm10p0, 1) + "ng/m3");
    tft.setCursor(10, 150);  
    tft.println("vocI: " + String(voc, 1));
    tft.setCursor(10, 170);  
    tft.println("noxI: " + String(nox, 1));

    String line;
    line += "PM1.0=" + String(pm1p0, 1);
    line += " PM2.5=" + String(pm2p5, 1);
    line += " PM4.0=" + String(pm4p0, 1);
    line += " PM10=" + String(pm10p0, 1);
    line += " RH=" + String(rh, 1);
    line += " T=" + String(t, 1);
    line += " VOC=" + String(voc, 1);
    line += " NOx=" + String(nox, 1);
    line += " CO2=" + String(co2);
    logBoth(line);
  }

  delay(1000);
}