// HC-SR04: VCC - 5V, trigger - 3V
// WeMos D1 (ESP8266): D1 - trigger, D2 - echo, D0 and RST connected for deep sleep mode
// MQTT using https://github.com/Imroy/pubsubclient
// JSON using: https://github.com/bblanchon/ArduinoJson
// config upload: https://github.com/esp8266/arduino-esp8266fs-plugin

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FS.h>



#define trigPin 5 //D1
#define echoPin 4 //D2

#define STATE_UNKNOWN 0
#define STATE_UNOCCUPIED 10
#define STATE_OCCUPIED 20

#define UNOCCUPIED_DISTANCE 80
#define VERSION 1
const int sleepTime = 1; // in seconds

// Samples configuration
const int samplesPerMeasure = 10;
const int maxSamplesPerMeasure = 50;
const int hysteresisBufferSize = 10;
boolean hysteresisBuffer[hysteresisBufferSize];
int hysteresisBufferIterator = 0;

// Config file
const String configFile = "/config.json";

/*
 * MQTT const variables - they need to be here 
 * we read them once in setup() and use them many times in loop()
 */
const char *mqttServer;
const char *mqttUser;
const char *mqttPassword;
const char *officeId;
StaticJsonBuffer<256> configJsonBuffer;
  
void setup() {
  Serial.begin (9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Read config file
  Serial.println("Reading config");

  JsonObject& jsonConf = readConfig(configFile, configJsonBuffer);
  
  const char* wifiSSID = jsonConf["SSID"];
  const char* wifiPassword = jsonConf["wifiPassword"];

  mqttServer = jsonConf["mqttServer"];
  mqttUser = jsonConf["mqttUser"];
  mqttPassword = jsonConf["mqttPassword"];
  
  officeId = jsonConf["officeId"];
  
  //Fill buffer with unknown state
  for (int i=0; i<hysteresisBufferSize; i++) {
    hysteresisBuffer[i] = STATE_UNKNOWN; //unknown state
  }

  // WiFi setup
  WiFi.begin(wifiSSID, wifiPassword);
  
  while (WiFi.status() != WL_CONNECTED) {
    
    delay(5000);
    Serial.println("[?] Waiting for WiFi connection");
  }
  Serial.println("");
  Serial.print("[*] WiFi connected [");
  Serial.print(WiFi.localIP());
  Serial.println("]");
}

/**
 * Main loop take measures and send to broker
 */
void loop() {
  int distance;
  int deviceId;
  int state;

  Serial.println("---------------------------------");
   
  deviceId = ESP.getChipId();
   
  distance = measureDistance();
   
  Serial.print("Distance: ");
  Serial.println(distance);

  state = getHysteresisState(distance);
  
  Serial.print("Hysteresis state: ");
  Serial.println(state);
  
  StaticJsonBuffer<256> messageJsonBuffer;
  JsonObject& jsonMessage = buildJson(messageJsonBuffer, distance, deviceId, VERSION, state);
  String message;
  jsonMessage.printTo(message);

  // send to mqtt
  mqttPublish(message, deviceId, officeId);

  // nobody is that quick in toilet ...
  delay(sleepTime * 1000);
}

void mqttCallback(const MQTT::Publish& pub) {
  // handle message arrived
}

/**
 * Send data to MQTT broker
 */
void mqttPublish(String message, int intDeviceId, String officeId) {
  WiFiClient wclient;
  PubSubClient client(wclient, mqttServer);
  
  if (WiFi.status() == WL_CONNECTED) {
    String deviceId = String(intDeviceId);
    
    // if not connected -> connect and publish
    if (!client.connected()) {
      Serial.println("Connecting to MQTT server");
      if (client.connect(MQTT::Connect(deviceId)
        .set_auth(mqttUser, mqttPassword)
        .set_keepalive(60)))
      {
        Serial.println("Connected to MQTT server");
        client.set_callback(mqttCallback);
        client.publish(MQTT::Publish("toilet/"+officeId+"/"+deviceId,message)
          .set_qos(1)
          .set_retain()
        );
      } else {
        Serial.println("Could not connect to MQTT server");
      }
    }
    // if connected -> publish
    else {
        Serial.println("Connected to MQTT server");
        client.set_callback(mqttCallback);
        client.publish(MQTT::Publish("toilet/"+officeId+"/"+deviceId,message)
          .set_qos(1)
          .set_retain()
        );
    }

    // console debug
    Serial.println(message);
  }
}

/**
 * Creates comunication frame
 */
JsonObject& buildJson(StaticJsonBufferBase& messageJsonBuffer, int distance, int deviceId, int ver, int state)  {
  JsonObject& root = messageJsonBuffer.createObject();

  root["deviceId"] = deviceId;
  root["status"] = state;
  root["version"] = ver;
  
  JsonArray& measurements = root.createNestedArray("measurements");
  JsonObject& nested = measurements.createNestedObject();
  nested["distance"] = distance;

  return root;
}

/**
 * Determinate state and apply histeresis using history states
 */
int getHysteresisState(int lastMeasurement) {
  int state = STATE_UNKNOWN;

  if (lastMeasurement) {
    
    if (lastMeasurement < UNOCCUPIED_DISTANCE) {
     state = STATE_OCCUPIED;
    } else {
     state = STATE_UNOCCUPIED;
    }
  }

  Serial.print("Current state ");
  Serial.println(state);

  hysteresisBuffer[hysteresisBufferIterator] = state;

  hysteresisBufferIterator = (hysteresisBufferIterator + 1) % hysteresisBufferSize;

  return getPopularElement();
}

/**
 * Return most popular state from buffer
 */
int getPopularElement() {
  int count = 1, tempCount;
  int popular = hysteresisBuffer[0];
  int temp = 0;

  Serial.print("Buffer ");
  
  for (int i = 0; i < hysteresisBufferSize; i++)
  {
    temp = hysteresisBuffer[i];
    tempCount = 0;

    Serial.print(temp);
    Serial.print(" ");
    
    for (int j = 1; j < hysteresisBufferSize; j++) {
      if (temp == hysteresisBuffer[j])
        tempCount++;
    }
    
    if (tempCount > count) {
      popular = temp;
      count = tempCount;
    }
  }

  Serial.println();
  
  return popular;
}

/**
 * Make multiple samples and calculat avg measure
 */
int measureDistance()  {

  int sampleCount = samplesPerMeasure,
      maxSampleCount = maxSamplesPerMeasure,
      measurements[samplesPerMeasure],
      sum = 0,
      average;
  
  Serial.print("Samples: ");

  while (sampleCount && maxSampleCount) {
    int distance = singleMeasure();

    Serial.print(" ");
    Serial.print(distance);

    if (distance) {
      sampleCount--;
      measurements[sampleCount] = distance;
      sum += distance;
    }
    maxSampleCount--;
  }

  Serial.println();

  int samplesCount = (samplesPerMeasure - sampleCount);

  if (samplesCount) {
    average = sum / samplesCount;
  } else {
    average = 0;
  }

  return average;
}

/**
 * Measure the distance using HC-SR04 sensor 
 * Return: distance (int) in cm
 */
int singleMeasure() {
  
  long time, distance;

  // generate triger
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(1);
  digitalWrite(trigPin, LOW);

  // measure time
  time = pulseIn(echoPin, HIGH, 30000);

  // calculate distance
  distance = time / 58;

  return distance;
}

/*
 * Read config from SPIFFS and return parsed json
 */
JsonObject& readConfig(String configFile, StaticJsonBufferBase& configJsonBuffer) {
  String payload;

  SPIFFS.begin(); // mount filesystem
  
  //read raw data from config file
  if (SPIFFS.exists(configFile)) {
    Serial.println("Config file found:");
    File file = SPIFFS.open(configFile,"r");
    if (file) {
      payload=file.readString();
      Serial.println(payload);
      //Serial.print(String(file.size()));        
    }
    file.close();
  } else {
    Serial.println("Config file not fould !"); 
  }

  // parsing json
  JsonObject& root = configJsonBuffer.parseObject(payload);
  
  SPIFFS.end(); // unmount filesystem

  return root;
}

