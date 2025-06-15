#include <OneWire.h>
#include <DallasTemperature.h>

const int oneWireBus = 4;  // GPIO4
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Deklarasikan variabel deviceAddress
DeviceAddress deviceAddress; 

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("Mencari sensor DS18B20...");
  sensors.begin();
  
  // Sekarang deviceAddress sudah terdeklarasi
  if (!sensors.getAddress(deviceAddress, 0)) {
    Serial.println("Error: Sensor tidak ditemukan!");
  } else {
    Serial.println("Sensor DS18B20 terdeteksi");
    Serial.print("Alamat Sensor: ");
    for (uint8_t i = 0; i < 8; i++) {
      Serial.print(deviceAddress[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    sensors.setResolution(12);
  }
}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Pembacaan gagal!");
  } else {
    Serial.print("Suhu: ");
    Serial.print(tempC);
    Serial.println(" °C");
  }
  delay(1000);
}
