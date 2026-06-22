#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "HuaweiPC";
const char* password = "Refilwe06";
const char* serverName = "http://192.168.137.1:5000/data";

int sensorPin = 39;
int sensorValue = 0;
int greenled = 14;
int redled = 17;

void setup() {
  Serial.begin(115200);
  pinMode(greenled, OUTPUT);
  pinMode(redled, OUTPUT);

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
  sensorValue = analogRead(sensorPin);
  sensorValue = map(sensorValue, 3200, 1500, 0, 100);
  sensorValue = constrain(sensorValue, 0, 100);

  Serial.print("Soil Moisture: ");
  Serial.print(sensorValue);
  Serial.println(" %");

  if (sensorValue > 80) {
    digitalWrite(greenled, HIGH);
    digitalWrite(redled, LOW);
  } else if (sensorValue < 40) {
    digitalWrite(greenled, LOW);
    digitalWrite(redled, HIGH);
  } else {
    digitalWrite(greenled, LOW);
    digitalWrite(redled, LOW);
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"id\":\"soil\", \"distance\":" + String(sensorValue) + "}";

    int httpResponseCode = http.POST(jsonData);
    Serial.print("HTTP Response: ");
    Serial.println(httpResponseCode);
    http.end();
  }

  delay(1000);
}
