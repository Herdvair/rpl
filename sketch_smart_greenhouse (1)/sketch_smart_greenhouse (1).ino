#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include <DHT_U.h>
#include <LightDependentResistor.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "redminote"
#define WIFI_PASSWORD "connect1234"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCRasveAyDD8FhV8cRyZ9uYPnNx7v7JfcE"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "triindah579@gmail.com"
#define USER_PASSWORD "triindah579"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://green-house2-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Variables to save database paths
String databasePath;
String systemPath;

String actuatorsPath;
String coolerPath;
String heaterPath;
String humidifierPath;
String lampPath;

String sensorsPath;
String tempPath;
String humPath;
String lightPath;

String preferencesPath;
String loTempPath;
String hiTempPath;
String loHumPath;
String loLightPath;

// DHT11 sensor
#define DHTPIN 19     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);
float temperature;
float humidity;
void readTemperature();
void readHumidity();

// LDR sensor
#define OTHER_RESISTOR 10000 //ohms
#define USED_PIN 36
#define USED_PHOTOCELL LightDependentResistor::GL5516
LightDependentResistor photocell(USED_PIN, OTHER_RESISTOR, USED_PHOTOCELL, 12, 12);
float illuminance;
void readIlluminance();

// Controlling's
#define NORMALLY_OPEN_RELAY true
void TurnOn(String actuator_switch); // function to turn the actuator on
void TurnOff(String actuator_switch); // function to turn the actuator off
int cooler = 0;
int heater = 0;
int humidifier = 0;
int lamp = 0;
#define COOLER 26
#define HEATER 25
#define HUMIDIFIER 33
#define LAMP 32
void getActuatorsData();
void setActuatorspinOut();
void controlTemperature();
void controlHumidity();
void controlIlluminance();

// Preferences local variable and functions needed
float lowTemperature;
float highTemperature;
float lowHumidity;
float lowIlluminance;
void getPreferences();

// Timer variables (send new readings every five seconds)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 5000;

// Initialise Wi-Fi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Write values to the database
void sendIntValue();
void sendFloat();

// Read values from the database
void getFloatValue();

void setup(){
  Serial.begin(115200);

  // Set all the acttuators pin as output
  pinMode(COOLER, OUTPUT);
  pinMode(HEATER, OUTPUT);
  pinMode(HUMIDIFIER, OUTPUT);
  pinMode(LAMP, OUTPUT);

  setActuatorspinOut();

  // Initialize DHT11 sensor then initialise Wi-Fi
  dht.begin();
  initWiFi();

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid;
  systemPath = databasePath + "/system";

  // Update database path for actuators, sensors and preferences groups
  actuatorsPath = systemPath + "/actuators";
  sensorsPath = systemPath + "/sensors";
  preferencesPath = systemPath + "/preferences";

  // Update database path for actuator's condition
  coolerPath = actuatorsPath + "/cooler";
  heaterPath = actuatorsPath + "/heater";
  humidifierPath = actuatorsPath + "/humidifier";
  lampPath = actuatorsPath + "/lamp";

  // Update database path for sensor readings
  tempPath = sensorsPath + "/temperature";
  humPath = sensorsPath + "/humidity";
  lightPath = sensorsPath + "/illuminance";

  // Update database path for the preferences
  loTempPath = preferencesPath + "/lowTemperature";
  hiTempPath = preferencesPath + "/highTemperature";
  loHumPath = preferencesPath + "/lowHumidity";
  loLightPath = preferencesPath + "/lowIlluminance";

  // Get initial actuator's contidions from the database and set the it's pin according to the conditions from the database
  getActuatorsData();
  setActuatorspinOut();
}

void loop(){
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    // Get latest preferences from the database
    getPreferences();

    // Get latest sensor readings
    readTemperature();
    readHumidity();
    readIlluminance();

    // Send readings to the database:
    sendFloatValue(tempPath, temperature);
    sendFloatValue(humPath, humidity);
    sendFloatValue(lightPath, illuminance);

    // Controlling the environmental conditions
    controlTemperature();
    controlHumidity();
    controlIlluminance();
  }
}

void getPreferences(){
  lowTemperature = getFloatValue(loTempPath);
  highTemperature = getFloatValue(hiTempPath);
  lowHumidity = getFloatValue(loHumPath);
  lowIlluminance = getFloatValue(loLightPath);
}

void getActuatorsData(){
  cooler = getIntValue(coolerPath);
  heater = getIntValue(heaterPath);
  humidifier = getIntValue(humidifierPath);
  lamp = getIntValue(lampPath);
}

void setActuatorspinOut(){
  if (cooler == 0) {
    TurnOff(COOLER);
  } else {
    TurnOn(COOLER);
  }
  if (heater == 0) {
    TurnOff(HEATER);
  } else {
    TurnOn(HEATER);
  }
  if (humidifier == 0) {
    TurnOff(HUMIDIFIER);
  } else {
    TurnOn(HUMIDIFIER);
  }
  if (lamp == 0) {
    TurnOff(LAMP);
  } else {
    TurnOn(LAMP);
  }
}

void readTemperature(){
  // Read temperature
  temperature = dht.readTemperature();
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(temperature)) {
    Serial.println(F("Failed to read temperature!"));
    return;
  }
  
  Serial.print(F("Temp: "));
  Serial.print(temperature);
  Serial.print(F("°C"));
}
  
void readHumidity(){
  // Read humidity
  humidity = dht.readHumidity();
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity)) {
    Serial.println(F("Failed to read humidity!"));
    return;
  }
  
  Serial.print(F(" Hum: "));
  Serial.print(humidity);
  Serial.print(F("%"));
}

void readIlluminance(){
  // Read illuminance
  illuminance = photocell.getCurrentLux();

  if (isnan(illuminance)) {
    Serial.println(F("Failed to read illuminance!"));
    return;
  }
  
  Serial.print(F("  Light: "));
  Serial.print(illuminance);
  Serial.println(F(" lx"));
}

void controlTemperature(){
  if (temperature >= lowTemperature && temperature <= highTemperature) {
    if (cooler != 0) {
      TurnOff(COOLER);
      cooler = 0;
      sendIntValue(coolerPath, cooler);
      Serial.println("Cooler turned off");
    }
    if (heater != 0) {
      TurnOff(HEATER);
      heater = 0;
      sendIntValue(heaterPath, heater);
      Serial.println("Heater turned off");
    }
  }
  else if (temperature < lowTemperature) {
    if (cooler != 0) {
      TurnOff(COOLER);
      cooler = 0;
      sendIntValue(coolerPath, cooler);
      Serial.println("Cooler turned off");
    }
    if (heater != 1) {
      TurnOn(HEATER);
      heater = 1;
      sendIntValue(heaterPath, heater);
      Serial.println("Heater turned on");
    }
  }
  else 
  {
    if (cooler != 1) {
      TurnOn(COOLER);
      cooler = 1;
      sendIntValue(coolerPath, cooler);
      Serial.println("Cooler turned on");
    }
    if (heater != 0) {
      TurnOff(HEATER);
      heater = 0;
      sendIntValue(heaterPath, heater);
      Serial.println("Heater turned off");
    }
  }
}

void controlHumidity(){
  if (humidity < lowHumidity) {
    if (humidifier != 1) {
      TurnOn(HUMIDIFIER);
      humidifier = 1;
      sendIntValue(humidifierPath, humidifier);
      Serial.println("Humidifier turned on");
    }
  }
  else {
    if (humidifier != 0) {
      TurnOff(HUMIDIFIER);
      humidifier = 0;
      sendIntValue(humidifierPath, humidifier);
      Serial.println("Humidifier turned off");
    }
  }
}

void controlIlluminance(){
  if (illuminance < lowIlluminance) {
    if (lamp != 1) {
      TurnOn(LAMP);
      lamp = 1;
      sendIntValue(lampPath, lamp);
      Serial.println("Lamp turned on");
    }
  }
  else {
    if (lamp != 0) {
      TurnOff(LAMP);
      lamp = 0;
      sendIntValue(lampPath, lamp);
      Serial.println("Lamp turned off");
    }
  }
}

float getFloatValue(String path){
  float temp_value;
  if (Firebase.RTDB.getFloat(&fbdo, path.c_str())){
    if (fbdo.dataType() == "float") {
      temp_value = fbdo.floatData();
      Serial.println(temp_value);
      return temp_value;
    }
    else {
      temp_value = (float) fbdo.floatData();
      Serial.println(temp_value);
      return temp_value;
    }
  }
  else {
    Serial.println(fbdo.errorReason());
  }
}

int getIntValue(String path){
  int temp_value;
  if (Firebase.RTDB.getInt(&fbdo, path.c_str())){
    if (fbdo.dataType() == "int") {
      temp_value = fbdo.intData();
      Serial.println(temp_value);
      return temp_value;
    }
    else {
      temp_value = (int) fbdo.intData();
      Serial.println(temp_value);
      return temp_value;
    }
  }
  else {
    Serial.println(fbdo.errorReason());
  }
}

void sendFloatValue(String path, float value){
  if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), value)){
    Serial.print("Writing value: ");
    Serial.print (value);
    Serial.print(" on the following path: ");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

void sendIntValue(String path, int value){
  if(Firebase.RTDB.setInt(&fbdo, path.c_str(), value)) {
    Serial.print("Writing value: ");
    Serial.print (value);
    Serial.print(" on the following path: ");
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

void TurnOn(int actuator_switch){
  if (NORMALLY_OPEN_RELAY == true) {
    digitalWrite(actuator_switch, LOW);
  }
  else {
    digitalWrite(actuator_switch, HIGH);
  }
}

void TurnOff(int actuator_switch){
  if (NORMALLY_OPEN_RELAY == true) {
    digitalWrite(actuator_switch, HIGH);
  }
  else {
    digitalWrite(actuator_switch, LOW);
  }
}