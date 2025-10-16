#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// 🔧 Konfiguracja Wi-Fi
const char* ssid = "OldCamp 🏰";
const char* password = "OnionChopper";

// 🔧 Konfiguracja Firebase Realtime Database
const char* firebaseHost = "projekt-sp-default-rtdb.europe-west1.firebasedatabase.app";
const char* firebaseApiKey = "AIzaSyAJB3DxLtFFOvC2OpTxLAbpr4rJUfCyOwo";
const char* roomName = "ESP8266_Test"; // Nazwa pomieszczenia/czujnika

// 🔧 Konfiguracja Czasu (NTP)
WiFiUDP ntpUDP;
NTPClient czas(ntpUDP, "pool.ntp.org", 7200, 60000); // +2h dla CEST

void setup() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("--- ROZPOCZYNAM SETUP ---");

  // Łączenie z Wi-Fi
  Serial.print("Łączenie z Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Limit prób
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nPołączono z Wi-Fi. Adres IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nBŁĄD: Nie udało się połączyć z Wi-Fi!");
    // Tutaj możesz zaimplementować restart lub inną logikę, jeśli Wi-Fi nie działa
    // Na razie po prostu będziemy kontynuować i zobaczymy, co się stanie
  }


  // Inicjalizacja NTP
  Serial.println("Inicjalizacja NTP Client.");
  czas.begin();
  if (!czas.forceUpdate()) { // Sprawdź, czy NTP zaktualizowało czas
    Serial.println("OSTRZEŻENIE: Nie udało się zaktualizować czasu z NTP w setup().");
  } else {
    Serial.print("Czas z NTP ustawiony: ");
    Serial.println(czas.getFormattedTime());
  }
  Serial.println("--- SETUP ZAKOŃCZONY ---");
}

void loop() {
  Serial.println("\n--- ROZPOCZYNAM CYKL LOOP ---");
  Serial.print("Aktualizacja czasu z NTP... ");
  if (!czas.update()) {
    Serial.println("BŁĄD: Nie udało się zaktualizować czasu z NTP w loop().");
  } else {
    Serial.println("OK.");
  }


  // 🔄 Generuj losowe dane (dla celów testowych)
  float temperatura = random(200, 300) / 10.0; // 20.0–30.0°C
  float wilgotnosc = random(400, 600) / 10.0;  // 40.0–60.0%

  // 🔄 Formatuj bieżący czas do ISO 8601 (np. 2025-10-16T13:43:56+02:00)
  time_t rawTime = czas.getEpochTime();
  struct tm* timeinfo = localtime(&rawTime); // localtime() używa wewnętrznego offsetu NTPClient
  char isoTimestamp[30];
  sprintf(isoTimestamp, "%04d-%02d-%02dT%02d:%02d:%02d+02:00", // +02:00 hardcoded dla CEST
          timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
          timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  Serial.print("Wygenerowany timestamp: ");
  Serial.println(isoTimestamp);


  // 🔄 Przygotuj payload JSON
  String jsonPayload = "{";
  jsonPayload += "\"timestamp\":\"" + String(isoTimestamp) + "\",";
  jsonPayload += "\"temperatura\":" + String(temperatura, 1) + ","; // format z 1 miejscem po przecinku
  jsonPayload += "\"wilgotnosc\":" + String(wilgotnosc, 1);       // format z 1 miejscem po przecinku
  jsonPayload += "}";
  Serial.print("Wygenerowany JSON payload: ");
  Serial.println(jsonPayload);


  // 🔄 Wysyłanie do Firebase
  WiFiClientSecure client;
  client.setInsecure(); // ⚠️ NIEBEZPIECZNE W PRODUKCJI!

  // --- 1. Wysyłanie NAJNOWSZYCH DANYCH do /currentReadings/{roomName} (PUT request) ---
  String currentReadingsUrl = "/currentReadings/" + String(roomName) + ".json?auth=" + String(firebaseApiKey);
  Serial.print("Próba PUT do Firebase pod URL: ");
  Serial.println(currentReadingsUrl);

  if (!WiFi.isConnected()) {
    Serial.println("BŁĄD: Wi-Fi jest rozłączone przed PUT requestem.");
  } else if (client.connect(firebaseHost, 443)) {
    Serial.println("Nawiązano połączenie TLS z Firebase (currentReadings).");
    client.println("PUT " + currentReadingsUrl + " HTTP/1.1");
    client.println("Host: " + String(firebaseHost));
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonPayload.length());
    client.println();
    client.println(jsonPayload);
    Serial.println("PUT request wysłany.");

    // Odczyt odpowiedzi
    Serial.println("Oczekiwanie na odpowiedź Firebase (currentReadings)...");
    String responseHeader = "";
    while (client.connected() && !client.available()) {
      delay(10); // Czekaj na dane
    }
    while (client.available()) {
      responseHeader += client.readStringUntil('\n');
    }
    Serial.print("Nagłówek odpowiedzi (currentReadings): ");
    Serial.println(responseHeader);

    // Szukamy statusu HTTP, np. "HTTP/1.1 200 OK"
    if (responseHeader.indexOf("HTTP/1.1 200 OK") != -1) {
      Serial.println("PUT request zakończony SUKCESEM (200 OK).");
    } else {
      Serial.println("BŁĄD: PUT request zakończony niepowodzeniem lub innym statusem HTTP.");
    }
    client.stop(); // Zamknij połączenie
  } else {
    Serial.print("BŁĄD: Nie udało się nawiązać połączenia TLS z Firebase hostem ");
    Serial.println(firebaseHost);
  }

  delay(1000); // Krótka pauza między requestami

  // --- 2. Wysyłanie DANYCH HISTORYCZNYCH do /czujniki/{roomName} (POST request) ---
  String historicalReadingsUrl = "/czujniki/" + String(roomName) + ".json?auth=" + String(firebaseApiKey);
  Serial.print("Próba POST do Firebase pod URL: ");
  Serial.println(historicalReadingsUrl);

  if (!WiFi.isConnected()) {
    Serial.println("BŁĄD: Wi-Fi jest rozłączone przed POST requestem.");
  } else if (client.connect(firebaseHost, 443)) {
    Serial.println("Nawiązano połączenie TLS z Firebase (czujniki).");
    client.println("POST " + historicalReadingsUrl + " HTTP/1.1");
    client.println("Host: " + String(firebaseHost));
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonPayload.length());
    client.println();
    client.println(jsonPayload);
    Serial.println("POST request wysłany.");

    // Odczyt odpowiedzi
    Serial.println("Oczekiwanie na odpowiedź Firebase (czujniki)...");
    String responseHeader = "";
    while (client.connected() && !client.available()) {
      delay(10); // Czekaj na dane
    }
    while (client.available()) {
      responseHeader += client.readStringUntil('\n');
    }
    Serial.print("Nagłówek odpowiedzi (czujniki): ");
    Serial.println(responseHeader);

    if (responseHeader.indexOf("HTTP/1.1 200 OK") != -1) {
      Serial.println("POST request zakończony SUKCESEM (200 OK).");
    } else {
      Serial.println("BŁĄD: POST request zakończony niepowodzeniem lub innym statusem HTTP.");
    }
    client.stop(); // Zamknij połączenie
  } else {
    Serial.print("BŁĄD: Nie udało się nawiązać połączenia TLS z Firebase hostem ");
    Serial.println(firebaseHost);
  }

  Serial.println("--- CYKL LOOP ZAKOŃCZONY ---");
  Serial.println("--- Czekam 5 minut do następnego pomiaru ---");
  delay(300000); // 300000 ms = 5 minut
}
