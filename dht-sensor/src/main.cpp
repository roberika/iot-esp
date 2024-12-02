#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "DHT.h"
#include <NTPClient.h>
#include <Adafruit_Sensor.h>
 
#define WIFI_SSID "Wifi Rumah" 
#define WIFI_PASSWORD "1992kunci" 
#define DHT_TYPE DHT11  
#define FIREBASE_PROJECT_ID "dht-firebase-if51"
#define API_KEY "AIzaSyDwOZzT_L0wlNby5OrAscTdL8sBKDmZ1ik"
#define THRESHOLD_COLLECTION_ID "threshold"
#define RECORDS_COLLECTION_ID "records"
#define RECORDS_INTERVAL 60000
#define USER_EMAIL "dht-firebase@if51.mdp.ac.id"
#define USER_PASSWORD "janganlupo"
#define DATABASE_URL "https://dht-firebase-if51-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DATABASE_SECRET "seXLmFU5gfHCwQ8SE4Y9yK1iDxLv1CAMJZtAkTed"
#define MONITORING_INTERVAL 1000
void authHandler();
String getTimestampString(uint64_t sec, uint32_t nano);

// TODO: Belum di cek pin berapa
#define BUZZER 4;
#define DHT_LEFT 4;
#define DHT_RIGHT 4;

DHT dhtLeft(DHT_LEFT, DHT_TYPE);
DHT dhtRight(DHT_RIGHT, DHT_TYPE);

DefaultNetwork network; 
FirebaseApp app;
UserAuth userAuth(API_KEY, USER_EMAIL, USER_PASSWORD);
LegacyToken legacyToken(DATABASE_SECRET);
RealtimeDatabase database;
AsyncResult resultRealtime;
Firestore::Documents docs;
WiFiClientSecure sslFirestore, sslRealtime;
NTPClient timeClient(sslFirestore, "pool.ntp.org");
AsyncClientClass clientFirestore(sslFirestore, getNetwork(network)), clientRealtime(sslRealtime, getNetwork(network));

double thresholdDHTLeftTemperature;
double thresholdDHTLeftHumidity;
double recordedDHTLeftTemperature;
double recordedDHTLeftHumidity;
double thresholdDHTRightTemperature;
double thresholdDHTRightHumidity;
double recordedDHTRightTemperature;
double recordedDHTRightHumidity;
int documentCount;
unsigned long lastAuthentication = 0;
unsigned long lastFirestoreUpdate = 0;
unsigned long lastRealtimeUpdate = 0;

void setup() { 
  // Setup perangkat
  Serial.begin(9600); 
  pinMode(BUZZER, OUTPUT);
  dhtLeft.begin();
  dhtRight.begin();
 
  // Konek ke internet 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  Serial.print("Connecting to "); 
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) { 
    Serial.print("."); 
    delay(500); 
  } 
  Serial.println(); 
  Serial.print("Connected to "); 
  Serial.println(WiFi.localIP()); 

  // Konek ke server NTP
  timeClient.begin();
  
  // Konek ke Firebase
  //.. Bangun header dan inisialisasi
  sslFirestore.setInsecure();
  sslFirestore.setBufferSizes(1024, 1024);
  initializeApp(clientFirestore, app, getAuth(userAuth));
  sslRealtime.setInsecure();
  sslRealtime.setBufferSizes(1024, 1024);
  initializeApp(clientRealtime, app, getAuth(legacyToken));

  //.. Konek ke stream realtime database
  authHandler();
  app.getApp<RealtimeDatabase>(database);
  database.url(DATABASE_URL);
  clientRealtime.setAsyncResult(resultRealtime);

  //.. Tes koneksi ke Firebase Project
  Serial.print("Connecting to "); 
  Serial.print(FIREBASE_PROJECT_ID);
  while (!app.ready()) {
    Serial.print("."); 
    delay(500); 
  }
  Serial.println(); 
  Serial.print("Connected to "); 
  Serial.println(FIREBASE_PROJECT_ID); 
} 
 
void loop() {
  authHandler();
  app.loop();

  //.. TODO: Ambil jumlah record pada Firebase
  if(!documentCount) {
    documentCount = 0;
  }

  // Ambil data suhu dan kelembaban dari dht
  recordedDHTLeftTemperature = dhtLeft.readTemperature(false);
  recordedDHTLeftHumidity = dhtLeft.readHumidity();
  recordedDHTRightTemperature = dhtRight.readTemperature(false);
  recordedDHTRightHumidity = dhtRight.readHumidity();

  // Ambil nilai threshold yang disimpan pada Firebase
  //.. Ambil Document dht1 dan dht2 pada Collection 
  //.. threshold yang berisi threshold peringatan buzzer
  String payload = docs.list(clientFirestore, Firestore::Parent(FIREBASE_PROJECT_ID), THRESHOLD_COLLECTION_ID);

  //.. Tampilkan payload atau error
  if (clientFirestore.lastError().code() == 0) Serial.println(payload);

  //.. TODO: Muat threshold
  thresholdDHTLeftTemperature = 30;
  thresholdDHTLeftHumidity = 30;
  thresholdDHTRightTemperature = 30;
  thresholdDHTRightHumidity = 30;

  // Cek apakah suhu dan kelembaban melebihi nilai threshold
  if((recordedDHTLeftHumidity >= thresholdDHTLeftHumidity && recordedDHTLeftTemperature >= thresholdDHTLeftTemperature) ||
    (recordedDHTRightHumidity >= thresholdDHTRightHumidity && recordedDHTRightTemperature >= thresholdDHTRightTemperature)) {
    digitalWrite(BUZZER, HIGH);
  } else {
    digitalWrite(BUZZER, LOW);
  }

  // Simpan rekaman catatan ke Realtime Database
  if (millis() - lastRealtimeUpdate > MONITORING_INTERVAL) {
    lastRealtimeUpdate = millis();
    database.set<double>(clientRealtime, "/leftHumidity", recordedDHTLeftHumidity);
    database.set<double>(clientRealtime, "/leftTemperature", recordedDHTLeftTemperature);
    database.set<double>(clientRealtime, "/rightHumidity", recordedDHTRightHumidity);
    database.set<double>(clientRealtime, "/rightTemperature", recordedDHTRightTemperature);
  }

  // Simpan rekamanan catatan ke Firestore Database
  if (millis() - lastFirestoreUpdate > RECORDS_INTERVAL) {
    lastFirestoreUpdate = millis();
    for(int i = 0; i < 2; i++) {
      //.. TODO: Susun jadi 1 dokumen
      Values::TimestampValue recordTime(getTimestampString(getTime(), 999999999));
      Values::DoubleValue recordTemperature((i == 0) ? recordedDHTLeftTemperature : recordedDHTRightTemperature);
      Values::DoubleValue recordHumidity((i == 0) ? recordedDHTLeftHumidity : recordedDHTRightHumidity);
      Values::IntegerValue recordDHTID(i);
      Document<Values::Value> doc("dhtid", Values::Value(recordDHTID));
      doc.add("time", Values::Value(recordTime));
      doc.add("temperature", Values::Value(recordTemperature));
      doc.add("humidity", Values::Value(recordHumidity));

      //.. Buat ID baru untuk dokumen, jadikan payload, dan kirim
      // WARN: Aku belum tahu documentPath to termasuk nama document atau tidak
      // Kalau belum termasuk dan namanya digenerate Firebase, hilangkan documentCount
      String documentPath = RECORDS_COLLECTION_ID + String(documentCount);
      String payload = docs.createDocument(clientFirestore, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc);
      documentCount++;
    }
  }

  // Delay supaya tidak spam 10 milidetik
  delay(10);
}

// Untuk autentikasi
void authHandler()
{
  // Blocking authentication handler with timeout
  unsigned long ms = millis();
  while (app.isInitialized() && !app.ready() && millis() - lastAuthentication < 120 * 1000)
  {
    // The JWT token processor required for ServiceAuth and CustomAuth authentications.
    // JWT is a static object of JWTClass and it's not thread safe.
    // In multi-threaded operations (multi-FirebaseApp), you have to define JWTClass for each FirebaseApp,
    // and set it to the FirebaseApp via FirebaseApp::setJWTProcessor(<JWTClass>), before calling initializeApp.
    JWT.loop(app.getAuth());
  }
}

unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

// Untuk timestamp
String getTimestampString(uint64_t sec, uint32_t nano)
{
    if (sec > 0x3afff4417f)
        sec = 0x3afff4417f;

    if (nano > 0x3b9ac9ff)
        nano = 0x3b9ac9ff;

    time_t now;
    struct tm ts;
    char buf[80];
    now = sec;
    ts = *localtime(&now);

    String format = "%Y-%m-%dT%H:%M:%S";

    if (nano > 0)
    {
        String fraction = String(double(nano) / 1000000000.0f, 9);
        fraction.remove(0, 1);
        format += fraction;
    }
    format += "Z";

    strftime(buf, sizeof(buf), format.c_str(), &ts);
    return buf;
}