#define trigPin 5 //D1
#define echoPin 4 //D2

void setup() {
  Serial.begin (9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

// VCC 5V, trigger 3V

void loop() {
  long czas, dystans;

  // measure
  digitalWrite(trigPin, LOW);
  delay(2);
  digitalWrite(trigPin, HIGH);
  delay(1);
  digitalWrite(trigPin, LOW);

  czas = pulseIn(echoPin, HIGH);
  //Serial.println(czas);
  dystans = czas / 58;

  Serial.print("Dystans: ");
  Serial.println(dystans);

  delay(500);
}
