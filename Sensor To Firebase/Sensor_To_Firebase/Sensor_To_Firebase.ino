#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <HX711_ADC.h>
#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// WiFi credentials
#define WIFI_SSID "Indonesia"
#define WIFI_PASSWORD "indonesia"

// Firebase configuration
#define API_KEY "AIzaSyAvfpMQNu-t6pyPN1JvudVlZ4bDqLR_KKc"
#define DATABASE_URL "https://kobonganmaszhe-default-rtdb.asia-southeast1.firebasedatabase.app"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// DS18B20 sensor pin
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
DeviceAddress deviceAddress;

// Relay pin
const int relayPin = 5;

// HX711 Load Cell
const int HX711_dout = 12;
const int HX711_sck = 13;
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;

// Timing
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Fuzzy logic membership functions
float temperatureLow(float temp) {
  if (temp <= 0) return 1.0;
  if (temp >= 60) return 0.0;
  return (60.0 - temp) / 60.0;
}

float temperatureHigh(float temp) {
  if (temp <= 60) return 0.0;
  return 1.0;
}

float weightLow(float weight) {
  if (weight <= 0) return 1.0;
  if (weight >= 174) return 0.0;
  return (174.0 - weight) / 174.0;
}

float weightHigh(float weight) {
  if (weight <= 174) return 0.0;
  if (weight >= 1000) return 1.0;
  return (weight - 174.0) / (1000.0 - 174.0);
}

void setup() {
  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected with IP: " + WiFi.localIP().toString());

  // Initialize DS18B20
  Serial.println("Mencari sensor DS18B20...");
  sensors.begin();
  if (!sensors.getAddress(deviceAddress, 0)) {
    Serial.println("Error: Sensor tidak ditemukan!");
  } else {
    Serial.println("Sensor DS18B20 terdeteksi");
    sensors.setResolution(12);
  }

  // Initialize Load Cell
  Serial.println("Inisialisasi Load Cell...");
  LoadCell.begin();
  float calibrationValue = -710.92;
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout: Cek koneksi HX711");
    while (1);
  } else {
    LoadCell.setCalFactor(calibrationValue);
    Serial.println("Load Cell terdeteksi");
  }

  // Firebase setup
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase anonymous signup OK");
    signupOK = true;
  } else {
    Serial.printf("Firebase signup failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 500 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Baca Temperature
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    if (temperature == DEVICE_DISCONNECTED_C) {
      Serial.println("Error: Pembacaan suhu gagal!");
      temperature = 0.0;
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");
    }

    // Baca berat
    float weight = 0.0;
    if (LoadCell.update()) {
      weight = LoadCell.getData();
      if (weight < 0) weight = 0;
      Serial.print("Berat: ");
      Serial.print(weight);
      Serial.println(" gram");
    } else {
      Serial.println("Load Cell belum siap, nilai sebelumnya digunakan");
    }

    // Baca Instruksi Fuzzy Control dari Firebase
    bool fuzzyEnabled = false;
    if (Firebase.RTDB.getBool(&fbdo, "kontrol/fuzzy")) {
      fuzzyEnabled = fbdo.boolData();
    } else {
      Serial.println("Gagal membaca kontrol/fuzzy: " + fbdo.errorReason());
    }

    // Fuzzy Control ON/OFF
    bool heaterState = false;
    if (fuzzyEnabled) {
      float tempLow = temperatureLow(temperature);
      float tempHigh = temperatureHigh(temperature);
      float wLow = weightLow(weight);
      float wHigh = weightHigh(weight);

      float ruleHeaterOff = min(tempHigh, wLow);
      float ruleHeaterOn = 1.0 - ruleHeaterOff;
      heaterState = (ruleHeaterOn >= ruleHeaterOff);

      Serial.print("Temp Low: "); Serial.print(tempLow);
      Serial.print(", Temp High: "); Serial.print(tempHigh);
      Serial.print(", Weight Low: "); Serial.print(wLow);
      Serial.print(", Weight High: "); Serial.println(wHigh);
    } else {
      Serial.println("Fuzzy belum aktif - Tekan tombol ON");
    }

    // Control Relay
    digitalWrite(relayPin, heaterState ? HIGH : LOW);
    Serial.println(heaterState ? "Relay: ON" : "Relay: OFF");

    // Kirim status relay ke Firebase
    Firebase.RTDB.setBool(&fbdo, "kontrol/relay", heaterState);

    // Kirim parameter monitoring
    Firebase.RTDB.setFloat(&fbdo, "monitoring/temperature", temperature);
    Firebase.RTDB.setFloat(&fbdo, "monitoring/weight", weight);
    
  }

  delay(200);
}
