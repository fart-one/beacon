// HC-SR04: VCC - 5V, trigger - 3V
// WeMos D1 (ESP8266): D1 - trigger, D2 - echo, D0 and RST connected for deep sleep mode
#define trigPin 5 //D1
#define echoPin 4 //D2

const int sleepTime = 5; // in seconds

void setup() {
  Serial.begin (9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}


void loop() {
  long czas, dystans;

  // measure
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(1);
  digitalWrite(trigPin, LOW);

  czas = pulseIn(echoPin, HIGH);
  Serial.println(czas);
  dystans = czas / 58;

  Serial.print("Distance: ");
  Serial.println(dystans);

  //ESP.deepSleep(sleepTime * 1000000);
}
