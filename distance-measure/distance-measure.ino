// HC-SR04: VCC - 5V, trigger - 3V
// WeMos D1 (ESP8266): D1 - trigger, D2 - echo, D0 and RST connected for deep sleep mode
// MQTT using https://github.com/Imroy/pubsubclient

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define trigPin 5 //D1
#define echoPin 4 //D2

#define UNOCCUPIED_DISTANCE 100
#define OFFICE "Sokratesa13"
const int sleepTime = 5; // in seconds

// WiFi
const char* SSID = "wifiSSID";
const char* wifiPassword = "wifipassword";
const char* mqttServer ="mqttserver";
const char* mqttUser = "mqttuser";
const char* mqttPassword = "mqttpassword";

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
  Serial.print("]");
}


void loop() {
  int distance;

  // distance = 0 measn measurement error
  do {
    distance = measureDistance();
  } while (distance == 0);
  
  Serial.print("Distance: ");
  Serial.println(distance);
  // send to mqtt
  
  if (distance < UNOCCUPIED_DISTANCE) {
    mqttPublish(1);
    delay(sleepTime * 1000);
  } else {
    mqttPublish(0);
  }
}


void callback(const MQTT::Publish& pub) {
  // handle message arrived
}

void mqttPublish(int stat) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.println("Connecting to MQTT server");
      if (client.connect(MQTT::Connect(String(ESP.getChipId())).set_auth(mqttUser, mqttPassword))) {
        Serial.println("Connected to MQTT server");
        client.set_callback(callback);
        client.publish("toilet/"+String(OFFICE)+"/"+String(ESP.getChipId()),String(stat));
      } else {
        Serial.println("Could not connect to MQTT server");   
      }
    }
    if (client.connected())
        Serial.println("Connected to MQTT server");
        client.set_callback(callback);
        client.publish("toilet/"+String(OFFICE)+"/"+String(ESP.getChipId()),String(stat));
  }

  String message = "toilet/"+String(OFFICE)+"/"+String(ESP.getChipId())+"/"+String(stat);
  Serial.println(message);
  }
}  

int measureDistance()  {
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
  time = pulseIn(echoPin, HIGH);

  // calculate distance
  distance = time / 58;

  return distance;
}

