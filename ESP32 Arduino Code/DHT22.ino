/*  DHT22 – Temperature, Humidity + Irrigation LED controller
 *
 *  This ESP32 does two things:
 *    1. Reads the DHT22 sensor and POSTs to Flask every 3 seconds
 *    2. Runs a tiny HTTP server on port 80 that listens for:
 *         POST /irrigate   body: {"irrigate": true}  → GREEN LED ON
 *         POST /irrigate   body: {"irrigate": false} → GREEN LED OFF
 *
 *  Flask sends the irrigate command whenever the irrigation
 *  decision changes (soil dry, no rain, tank full, no motion).
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "DHT.h"

const char* ssid       = "HuaweiPC";
const char* password   = "Refilwe06";
const char* serverName = "http://192.168.137.1:5000/data";

#define DHTPIN      16
#define DHTTYPE     DHT22r
#define TEMP_LED    26    // original temperature warning LED (red)
#define GREEN_LED   27    // irrigation status LED (GREEN = irrigating)

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);     // tiny HTTP server for receiving Flask commands

float temperature = 0.0;
float humidity    = 0.0;
bool  irrigating  = false;

unsigned long previousReadTime = 0;
unsigned long previousSendTime = 0;
const unsigned long READ_INTERVAL = 2500;
const unsigned long SEND_INTERVAL = 3000;


// ─── HTTP server handler: POST /irrigate ────────────────────────────────────
void handleIrrigateCommand() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<64> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) {
    server.send(400, "application/json", "{\"error\":\"Bad JSON\"}");
    return;
  }

  irrigating = doc["irrigate"].as<bool>();
  digitalWrite(GREEN_LED, irrigating ? HIGH : LOW);

  Serial.print("[IRRIGATE CMD] received irrigate=");
  Serial.println(irrigating ? "true → LED ON" : "false → LED OFF");

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ─── HTTP server handler: GET / (health check) ──────────────────────────────
void handleRoot() {
  String ip = WiFi.localIP().toString();
  server.send(200, "application/json",
    "{\"device\":\"DHT22_ESP32\",\"ip\":\"" + ip + "\",\"irrigating\":" +
    (irrigating ? "true" : "false") + "}");
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
bool postJSON(const char* url, const String& body) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  Serial.print("  HTTP code: ");
  Serial.println(code);
  http.end();
  return (code == 200);
}


void setup() {
  Serial.begin(115200);
  pinMode(TEMP_LED,  OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(TEMP_LED,  LOW);
  digitalWrite(GREEN_LED, LOW);

  dht.begin();

  // ── Connect to WiFi ────────────────────────────────────────────────────────
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!  IP: " + WiFi.localIP().toString());
  Serial.println(">> Flask should POST irrigation commands to: http://" +
                 WiFi.localIP().toString() + "/irrigate");

  // ── Start HTTP server ──────────────────────────────────────────────────────
  server.on("/",         HTTP_GET,  handleRoot);
  server.on("/irrigate", HTTP_POST, handleIrrigateCommand);
  server.begin();
  Serial.println("[SERVER] HTTP server started on port 80");
}


void loop() {
  // ── Always handle incoming HTTP requests first ─────────────────────────────
  server.handleClient();

  unsigned long now = millis();

  // ── READ DHT22 ─────────────────────────────────────────────────────────────
  if (now - previousReadTime >= READ_INTERVAL) {
    previousReadTime = now;

    float newHum  = dht.readHumidity();
    float newTemp = dht.readTemperature();

    if (isnan(newHum) || isnan(newTemp)) {
      Serial.println("[DHT22] Read failed – retrying next cycle");
    } else {
      humidity    = newHum;
      temperature = newTemp;
      Serial.printf("[DHT22] Temp: %.1f C   Humidity: %.1f %%\n", temperature, humidity);

      // Temperature warning LED (independent of irrigation LED)
      digitalWrite(TEMP_LED, temperature > 35 ? HIGH : LOW);
    }
  }

  // ── SEND TO FLASK ──────────────────────────────────────────────────────────
  if (now - previousSendTime >= SEND_INTERVAL) {
    previousSendTime = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Disconnected – reconnecting…");
      WiFi.reconnect();
      return;
    }

    if (temperature != 0.0 || humidity != 0.0) {
      Serial.println("[SEND] Temperature →");
      String jsonTemp = "{\"id\":\"temp\",\"distance\":" + String(temperature, 2) + "}";
      postJSON(serverName, jsonTemp);

      delay(150);

      Serial.println("[SEND] Humidity →");
      String jsonHum = "{\"id\":\"humidity\",\"distance\":" + String(humidity, 2) + "}";
      postJSON(serverName, jsonHum);
    }
  }
}
