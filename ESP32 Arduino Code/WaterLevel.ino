#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "HuaweiPC";
const char* password = "Refilwe06";

const char* serverName = "http://192.168.137.1:5000/data";

int trig = 5;
int echo = 18;

float D1 = 24.0;
float D2 = 0.0;
float D3 = 0.0;
float percentage = 0.0;

int redled = 26;
int yellowled = 25;
int greenled = 17;

// Timing
unsigned long previousReadTime = 0;
unsigned long previousSendTime = 0;

const int readInterval = 200;   // sensor + LED speed
const int sendInterval = 200;   // website live speed

void setup() {

  Serial.begin(115200);

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  pinMode(redled, OUTPUT);
  pinMode(yellowled, OUTPUT);
  pinMode(greenled, OUTPUT);

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());
}

void loop() {

  unsigned long currentMillis = millis();

  // FAST SENSOR + LED UPDATE
  if (currentMillis - previousReadTime >= readInterval) {

    previousReadTime = currentMillis;

    // Trigger ultrasonic
    digitalWrite(trig, LOW);
    delayMicroseconds(2);

    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    long duration = pulseIn(echo, HIGH, 30000);

    D2 = (duration * 0.0343) / 2;

    D3 = D1 - D2;

    if (D3 < 0) D3 = 0;

    percentage = (D3 / D1) * 100;

    if (percentage > 100) percentage = 100;

    Serial.print("Water Level: ");
    Serial.print(percentage);
    Serial.println(" %");

    // FAST LED SWITCHING
    if (percentage < 30) {

      digitalWrite(redled, HIGH);
      digitalWrite(yellowled, LOW);
      digitalWrite(greenled, LOW);

    }
    else if (percentage >= 30 && percentage <= 75) {

      digitalWrite(redled, LOW);
      digitalWrite(yellowled, HIGH);
      digitalWrite(greenled, LOW);

    }
    else {

      digitalWrite(redled, LOW);
      digitalWrite(yellowled, LOW);
      digitalWrite(greenled, HIGH);

    }
  }

  // SEND TO WEBSITE (LIVE)
  if (currentMillis - previousSendTime >= sendInterval) {

    previousSendTime = currentMillis;

    if (WiFi.status() == WL_CONNECTED) {

      HTTPClient http;

      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      String jsonData =
        "{\"id\":\"waterlevel\","
        "\"distance\":" + String(percentage) + "}";

      int httpResponseCode = http.POST(jsonData);

      Serial.print("HTTP Response: ");
      Serial.println(httpResponseCode);

      http.end();
    }
  }
}
