#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid       = "HuaweiPC";
const char* password   = "Refilwe06";
const char* serverName = "http://192.168.137.1:5000/data";

const int PIR_PIN = 14;
const int LED_PIN = 5;

unsigned long previousSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;   // send once per second

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!  IP: " + WiFi.localIP().toString());
}

void loop() {
  int motion = digitalRead(PIR_PIN);

  // ── Local LED feedback (instant, no delay) ────────────────
  digitalWrite(LED_PIN, motion == HIGH ? HIGH : LOW);
  Serial.print("[PIR] ");
  Serial.println(motion == HIGH ? "Motion DETECTED" : "No motion");

  // ── Send to Flask (throttled) ─────────────────────────────
  unsigned long now = millis();
  if (now - previousSendTime >= SEND_INTERVAL) {
    previousSendTime = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Disconnected – reconnecting…");
      WiFi.reconnect();
      return;
    }

    String motionStr = (motion == HIGH) ? "Detected" : "None";
    // Use "distance" key so Flask + Motion.html both receive it correctly
    String jsonData  = "{\"id\":\"pir_motion\",\"distance\":\"" + motionStr + "\"}";

    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonData);
    Serial.printf("[PIR] HTTP %d  status=%s\n", code, motionStr.c_str());
    http.end();
  }

  delay(100);   // short delay so we don't spam Serial while still being responsive
}
