#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WiFi credentials
#define WIFI_SSID "Indonesia"
#define WIFI_PASSWORD "indonesia"

// Firebase configuration
#define API_KEY "AIzaSyAvfpMQNu-t6pyPN1JvudVlZ4bDqLR_KKc"
#define DATABASE_URL "https://kobonganmaszhe-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Fuzzy logic membership functions
// Temperature Low (30 - 60 Derajat C)
// Temperature High (>60 Derajat C
float temperatureLow(float temp) {
  if (temp <= 30) return 1.0;        
  if (temp >= 60) return 0.0;        
  return (60.0 - temp) / (60.0 - 30.0); 
}

float temperatureHigh(float temp) {
  if (temp <= 60) return 0.0;        
  return 1.0;                        
}

// Fuzzy logic membership functions
// Weight Low (300 - 390 Gram)
// Weight High (391 - 500 Gram)
float weightLow(float weight) {
  if (weight <= 300) return 1.0;     
  if (weight >= 391) return 0.0;     
  return (391.0 - weight) / (391.0 - 300.0); 
}

float weightHigh(float weight) {
  if (weight <= 391) return 0.0;     
  if (weight >= 500) return 1.0;     
  return (weight - 391.0) / (500.0 - 391.0); 
}

void setup() {
  Serial.begin(9600);
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the database URL and API key */
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;

  /* Sign up anonymously */
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
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    
    // Generate random data
    float temperature = random(200, 301) / 10.0;  // 70.0 - 80.0
    float weight = random(2000, 3001) / 10.0;      // 200.0 - 300.0
    
    // Fuzzy logic membership values
    float tempLow = temperatureLow(temperature);
    float tempHigh = temperatureHigh(temperature);
    float wLow = weightLow(weight);
    float wHigh = weightHigh(weight);

    // Fuzzy rule: Heater OFF if temperature is HIGH AND weight is LOW
    float ruleHeaterOff = min(tempHigh, wLow);
    float ruleHeaterOn = 1.0 - ruleHeaterOff;

    // Defuzzification: Heater is OFF (false) if ruleHeaterOff > ruleHeaterOn
    bool heaterState = (ruleHeaterOn >= ruleHeaterOff);

    // Send control data
    if (Firebase.RTDB.setBool(&fbdo, "kontrol", heaterState)) {
      Serial.print("Heater state sent: ");
      Serial.println(heaterState ? "ON (true)" : "OFF (false)");
    } else {
      Serial.println("Failed to send control data");
      Serial.println("Error: " + fbdo.errorReason());
    }
    
    // Send temperature data
    if (Firebase.RTDB.setFloat(&fbdo, "monitoring/temperature", temperature)) {
      Serial.print("Temperature sent: ");
      Serial.println(temperature);
    } else {
      Serial.println("Failed to send temperature");
      Serial.println("Error: " + fbdo.errorReason());
    }

    // Send weight data
    if (Firebase.RTDB.setFloat(&fbdo, "monitoring/weight", weight)) {
      Serial.print("Weight sent: ");
      Serial.println(weight);
    } else {
      Serial.println("Failed to send weight");
      Serial.println("Error: " + fbdo.errorReason());
    }

    // For debugging: Print fuzzy membership values
    Serial.print("Temp Low: ");
    Serial.print(tempLow);
    Serial.print(", Temp High: ");
    Serial.print(tempHigh);
    Serial.print(", Weight Low: ");
    Serial.print(wLow);
    Serial.print(", Weight High: ");
    Serial.println(wHigh);
  }
  delay(1000);
}
