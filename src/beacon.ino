// HC-SR04: VCC - 5V, trigger - 3V
// WeMos D1 (ESP8266): D1 - trigger, D2 - echo, D0 and RST connected for deep sleep mode
// MQTT using https://github.com/Imroy/pubsubclient
// JSON using: https://github.com/bblanchon/ArduinoJson

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


#define trigPin 5 //D1
#define echoPin 4 //D2

#define UNOCCUPIED_DISTANCE 100
#define OFFICE "MeaningfulOfficeID"
#define VERSION 1
const int sleepTime = 5; // in seconds

// Samples configuration
const int samplesPerMeasure = 10;
const int maxSamplesPerMeasure = 50;
const int hysteresisBufferSize = 5;
boolean hysteresisBuffer[hysteresisBufferSize];

// WiFi
const char* SSID = "SSID";
const char* wifiPassword = "WiFi password";
const char* mqttServer ="mqtt FQDN";
const char* mqttUser = "mqtt login";
const char* mqttPassword = "mqtt password";

// MQTT
WiFiClient wclient;
PubSubClient client(wclient, mqttServer);

void setup() {
  Serial.begin (9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // WiFi setup
  WiFi.begin(SSID, wifiPassword);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("[?] Waiting for WiFi connection");
  }
  Serial.println("");
  Serial.print("[*] WiFi connected [");
  Serial.print(WiFi.localIP());
  Serial.println("]");
}


void loop() {
  int distance;
  int deviceId;
  int state;
  
  StaticJsonBuffer<256> jsonBuffer;
   
  deviceId = ESP.getChipId();
   
  distance = measureDistance();
   
  Serial.print("Distance: ");
  Serial.println(distance);

  if (getHysteresisState(distance)) {
    state = 10;
  } else {
    state = 0;
  }
  
  Serial.print("State: ");
  Serial.println(state);
  
  JsonObject& jsonMessage = buildJson(jsonBuffer, distance, deviceId, VERSION, state);
  String message;
  jsonMessage.printTo(message);
  

  // send to mqtt
  mqttPublish(message, deviceId);

  // nobody is that quick in toilet ...
  if (state)  {
    delay(sleepTime * 1000);
  }
}


void mqttCallback(const MQTT::Publish& pub) {
  // handle message arrived
}

void mqttPublish(String message, int intDeviceId) {
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
        client.publish(MQTT::Publish("toilet/"+String(OFFICE)+"/"+deviceId,message)
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
        client.publish(MQTT::Publish("toilet/"+String(OFFICE)+"/"+deviceId,message)
          .set_qos(1)
          .set_retain()
        );
    }

    // console debug
    Serial.println(message);
  }
}

JsonObject& buildJson(JsonBuffer& jsonBuffer, int distance, int deviceId, int ver, int state)  {
  JsonObject& root = jsonBuffer.createObject();

  root["deviceId"] = deviceId;
  root["status"] = state;
  root["version"] = ver;
  
  JsonArray& measurements = root.createNestedArray("measurements");
  JsonObject& nested = measurements.createNestedObject();
  nested["distance"] = distance;

  return root;
}

boolean getHysteresisState(int lastMeasurement) {
//@todo cyclic buffer for hysteresis
  
  if (lastMeasurement < UNOCCUPIED_DISTANCE) {
    return true;
  } else {
    return false;
  }
}

int measureDistance()  {

  int sampleCount = samplesPerMeasure,
      maxSampleCount = maxSamplesPerMeasure,
      measurements[samplesPerMeasure],
      sum = 0,
      average;

  while (sampleCount && maxSamplesPerMeasure) {
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

  int samplesCount = (samplesPerMeasure - sampleCount);

  if (samplesCount) {
    average = sum / samplesCount;
  } else {
    average = 0;
  }

 
  Serial.println();
  Serial.print("Average ");
  Serial.println(average);

  return average;
}

int singleMeasure() {
  
  /* Measure the distance using HC-SR04 sensor 
   Retur: distance (int) in cm */

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

