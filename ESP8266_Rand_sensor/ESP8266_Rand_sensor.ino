#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// 🔧 Wi-Fi
const char* ssid = "OldCamp 🏰";
const char* password = "OnionChopper";

// 🔧 Firebase REST API
const char* host = "projekt-sp-default-rtdb.europe-west1.firebasedatabase.app";
const char* apiKey = "AIzaSyAJB3DxLtFFOvC2OpTxLAbpr4rJUfCyOwo";
const char* czujnikID = "ESP8266_Test";

// 🔧 Czas
WiFiUDP ntpUDP;
NTPClient czas(ntpUDP, "pool.ntp.org", 7200, 60000); // +2h dla CEST

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Łączenie z Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nPołączono z Wi-Fi");
  czas.begin();
}

void loop() {
  czas.update();

  // 🔄 Generuj dane
  float temperatura = random(200, 300) / 10.0; // 20.0–30.0°C
  float wilgotnosc = random(400, 600) / 10.0;  // 40.0–60.0%

  // 🔄 Format ISO 8601 z +02:00
  time_t raw = czas.getEpochTime();
  struct tm* ti = localtime(&raw);
  char iso[30];
  sprintf(iso, "%04d-%02d-%02dT%02d:%02d:%02d+02:00",
          ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
          ti->tm_hour, ti->tm_min, ti->tm_sec);

  // 🔄 JSON payload
  String json = "{";
  json += "\"timestamp\":\"" + String(iso) + "\",";
  json += "\"temperatura\":" + String(temperatura, 1) + ",";
  json += "\"wilgotnosc\":" + String(wilgotnosc, 1);
  json += "}";

  // 🔄 Połączenie HTTPS
  WiFiClientSecure client;
  client.setInsecure(); // ⚠️ akceptuj wszystkie certyfikaty (dla uproszczenia)

  String url = "/czujniki/" + String(czujnikID) + ".json?auth=" + apiKey;

  Serial.print("⏳ Wysyłanie do Firebase: ");
  Serial.println(url);

  if (client.connect(host, 443)) {
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(json.length());
    client.println();
    client.println(json);

    // 🔄 Odczyt odpowiedzi
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }

    String response = client.readString();
    Serial.println("✔️ Odpowiedź Firebase: " + response);
  } else {
    Serial.println("❌ Błąd połączenia z Firebase");
  }

  client.stop();
  delay(300000); // co 10 sekund
}