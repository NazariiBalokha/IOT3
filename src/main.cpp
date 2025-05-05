# include <SPI.h>
# include <WiFiNINA.h>
# include <rgb_lcd.h>
# include <ArduinoMqttClient.h>
# include <ArduinoJson.h>
# include "Grove_Temperature_And_Humidity_Sensor.h"

// Temp and Humidity sensor
#define DHTTYPE DHT11
#define DHTPIN 4

// fan settings
int fanPin = 2;
int fanState = LOW;

DHT dht(DHTPIN, DHTTYPE);

// WiFi credentials
const char *ssid = "ioting";
const char *password = "stikiot1bajer";
 
// MQTT Broker details
unsigned long lastMessurment = millis();
unsigned long interval = 5000;

int counter = 0;
int revenseCounter = 0;

float tempInterval = 24.00;

bool started = false;

bool fanActivity = false;


// Grove LCD setup
rgb_lcd lcd;
 
// Network and MQTT client
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "192.168.1.125";
int port = 1883;

const char device_uid[] = "arduino";

const char tempTopic[]  = "arduino/temp";
const char humidityTopic[]  = "arduino/humidity";
const char fanTopic[] = "arduino/fan";

const char intervalGetTopic[]  = "arduino/get/interval";
const char tempGetTopic[] = "arduino/get/threshold";

const char intervalTopic[]  = "arduino/interval";
const char thresholdTopic[] = "arduino/threshold";


// Function prototypes
void setupWifi();
void setupMqtt();

void onMqttMessage(int messageSize);

void mqttPublishFloat(const char topic[], float value);
void mqttPublishBool(const char topic[], bool value);

void initialConfig();
void humidityAndTemp();
void startFan(float temp, float humidity);
 
void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.setRGB(0, 128, 64);
  lcd.print("Booting...");
 
  setupWifi();
  setupMqtt();

  Wire.begin();
  dht.begin();
  pinMode(fanPin, OUTPUT);
  
  mqttClient.poll();
  initialConfig();
  mqttClient.poll();
  lcd.setRGB(137, 207, 245);
}
 
void loop() {
  mqttClient.poll();
  humidityAndTemp();
}
 
void setupWifi() {
  lcd.clear();
  lcd.print("Connecting WiFi");
  
 
  WiFi.begin(ssid, password);
  int attempts = 0;
 
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    lcd.print(".");
    attempts++;
  }
 
  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.print("WiFi Connected");
    
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    lcd.clear();
    lcd.print("WiFi Failed");

  }
 
  delay(1000);
  lcd.clear();
}

void setupMqtt(){
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  mqttClient.onMessage(onMqttMessage);

  mqttClient.subscribe(intervalTopic); 
  mqttClient.subscribe(thresholdTopic); 
}

void mqttPublishFloat(const char topic[], float value){
  //create a json document
  StaticJsonDocument<128> doc;
  doc["value"] = value;
  doc["device_uid"] = device_uid;

  char jsonBuffer[128];
  serializeJson(doc, jsonBuffer); 

  mqttClient.beginMessage(topic);
  mqttClient.print(jsonBuffer);
  mqttClient.endMessage();
}

void mqttPublishBool(const char topic[], bool value){
  //create a json document
  StaticJsonDocument<128> doc;
  doc["value"] = value;
  doc["device_uid"] = device_uid;

  char jsonBuffer[128];
  serializeJson(doc, jsonBuffer);

  mqttClient.beginMessage(topic);
  mqttClient.print(jsonBuffer);
  mqttClient.endMessage();
}


void onMqttMessage(int messageSize) {
  if (mqttClient.available()) {
  
    String topic = mqttClient.messageTopic();
    String message = "";
    

    while (mqttClient.available()) {
      char c = (char)mqttClient.read();
      message += c;
    }
  
    Serial.print("Received on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    Serial.println(message);
    
    //topic on witch threshold is sent to 
    if (topic == thresholdTopic) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, message);
  
      if (!error) {
        tempInterval = doc["threshold"];
        Serial.print("Threshold updated to: ");
        Serial.println(tempInterval);
      } else {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      }
    }

    //topic on witch interval is sent to 
    if (topic == intervalTopic) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, message);
  
      if (!error) {
        interval = doc["interval"];
        Serial.print("Interval updated to: ");
        Serial.println(interval);
      } else {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      }
    }
  }

  Serial.println();
}

void initialConfig(){
  if(started == false){
    //restirve settings from db
    mqttClient.beginMessage(intervalGetTopic);
    mqttClient.print("");
    mqttClient.endMessage();
    
    mqttClient.beginMessage(tempGetTopic);
    mqttClient.print("");
    mqttClient.endMessage();
    
    started = true;
  }
}

void humidityAndTemp() {
  //Read the remperature from sensor
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  //Read update only when the interval
  unsigned long timeNow = millis();
  if (timeNow - lastMessurment >= interval){
    temp = dht.readTemperature();
    humidity = dht.readHumidity();
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Temp: ");
    lcd.print(temp);

    
    lcd.setCursor(0,1);
    lcd.print("Humidity: ");
    lcd.print(humidity);  
    lcd.print("%");
    
    lastMessurment = timeNow;

    mqttPublishFloat(tempTopic, temp);
    mqttPublishFloat(humidityTopic, humidity);

    startFan(temp, humidity);
  }
}

void startFan(float temp, float humidity){
  if (temp >= tempInterval){
    counter++;
    revenseCounter = 0;
  }else{
    counter = 0;
    revenseCounter++;
  }

  if (counter >= 10){
    fanState = HIGH;
    digitalWrite(fanPin, fanState);
    if(fanActivity == false){
      fanActivity = true;
      mqttPublishBool(fanTopic, fanActivity);
    }

    lcd.setRGB(255, 91, 97);
  }

  if (revenseCounter >= 5){
    fanState = LOW;
    digitalWrite(fanPin, fanState);
    if(fanActivity == true){
      fanActivity = false;
      mqttPublishBool(fanTopic, fanActivity);
    }

    lcd.setRGB(137, 207, 245);
  }
}

