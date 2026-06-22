#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "HuaweiPC";
const char* password = "Refilwe06";
const char* serverName = "http://192.168.137.1:5000/data";

int sensorPin = 39;
int sensorValue = 0;
int greenled = 27;

unsigned long previousSendTime = 0;
const long sendInterval = 5000; // send every 5 seconds

void setup() {
  Serial.begin(115200);
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
  sensorValue = analogRead(sensorPin);
  Serial.print("Rain Sensor Value: ");
  Serial.println(sensorValue);

  String status;
  if (sensorValue > 500) {
    status = "Raining";
    digitalWrite(greenled, HIGH);
  } else {
    status = "Not Raining";
    digitalWrite(greenled, LOW);
  }

  Serial.print("Status: ");
  Serial.println(status);

  if (millis() - previousSendTime >= sendInterval) {
    previousSendTime = millis();

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      String jsonData =
        "{\"id\":\"rainfall\","
        "\"value\":" + String(sensorValue) + ","
        "\"status\":\"" + status + "\"}";

      int httpResponseCode = http.POST(jsonData);
      Serial.print("HTTP Response: ");
      Serial.println(httpResponseCode);
      http.end();
    }
  }

  delay(200);
}
