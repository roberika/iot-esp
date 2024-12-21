#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <nlohmann/json.hpp>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "time.h"
 
#define WIFI_SSID "Wifi Rumah" 
#define WIFI_PASSWORD "1992kunci" 
#define DHT_TYPE DHT11  
#define FIREBASE_PROJECT_ID "dht-firebase-if51"
#define API_KEY "AIzaSyDwOZzT_L0wlNby5OrAscTdL8sBKDmZ1ik"
#define THRESHOLD_COLLECTION_ID "thresholds/threshold"
#define THRESHOLD_INTERVAL 5000
#define RECORDS_COLLECTION_ID "records"
#define RECORDS_INTERVAL 10000
#define USER_EMAIL "dht-firebase@if51.mdp.ac.id"
#define USER_PASSWORD "janganlupo"
#define DATABASE_URL "https://dht-firebase-if51-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DATABASE_SECRET "seXLmFU5gfHCwQ8SE4Y9yK1iDxLv1CAMJZtAkTed"
#define MONITORING_INTERVAL 1000
#define BUZZER 4
#define DHT_LEFT 14
#define DHT_RIGHT 5

// Having auth problems, switching to ServiceAuth tommorow
// Nevermind userAuth works
DHT dhtLeft(DHT_LEFT, DHT_TYPE);
DHT dhtRight(DHT_RIGHT, DHT_TYPE);
DefaultNetwork network; 
FirebaseApp appFirestore, appRealtime;
UserAuth userAuth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
LegacyToken legacyToken(DATABASE_SECRET);
RealtimeDatabase database;
AsyncResult resultFirestore, resultRealtime;
Firestore::Documents docs;
WiFiClientSecure sslFirestore, sslRealtime;
using AsyncClient = AsyncClientClass;
AsyncClient clientFirestore(sslFirestore, getNetwork(network)), clientRealtime(sslRealtime, getNetwork(network));
using json = nlohmann::json;

void authHandler(FirebaseApp appFirestore);
String getTimestampString(uint64_t sec);
unsigned long getTime();
void printPayload(AsyncClient client, String payload);
void printResult(AsyncResult &aResult);

double thresholdDHTLeftTemperature;
double thresholdDHTLeftHumidity;
double recordedDHTLeftTemperature;
double recordedDHTLeftHumidity;

double thresholdDHTRightTemperature;
double thresholdDHTRightHumidity;
double recordedDHTRightTemperature;
double recordedDHTRightHumidity;

unsigned long lastThresholdUpdate = 0;
unsigned long lastFirestoreUpdate = 0;
unsigned long lastRealtimeUpdate = 0;

void setup() { 
  // Setup perangkat
  Serial.begin(115200); 
  Serial.println(" ");
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
  //.. UTC+7 & no daylight savings
  configTime(0, 0, "id.pool.ntp.org");
  
  // Konek ke Firebase
  Serial.print("Connecting to "); 
  Serial.println(FIREBASE_PROJECT_ID); 

  //.. Konek ke cloud firestore
  sslFirestore.setInsecure();
  sslFirestore.setBufferSizes(1024, 1024);
  initializeApp(clientFirestore, appFirestore, getAuth(userAuth), resultFirestore);

  authHandler(appFirestore);
  Serial.println("Authentication Information");
  Firebase.printf("User UID: %s\n", appFirestore.getUid().c_str());
  Firebase.printf("Auth Token: %s\n", appFirestore.getToken().c_str());
  Firebase.printf("Refresh Token: %s\n", appFirestore.getRefreshToken().c_str());

  appFirestore.getApp<Firestore::Documents>(docs);
  clientFirestore.setAsyncResult(resultFirestore);

  //.. Konek ke stream realtime database
  sslRealtime.setInsecure();
  sslRealtime.setBufferSizes(1024, 1024);
  initializeApp(clientRealtime, appRealtime, getAuth(legacyToken));

  appRealtime.getApp<RealtimeDatabase>(database);
  database.url(DATABASE_URL);
  clientRealtime.setAsyncResult(resultRealtime);

  Serial.print("Connected to "); 
  Serial.println(FIREBASE_PROJECT_ID); 
} 
 
void loop() {
  
  if (!appFirestore.ready() || !appRealtime.ready()) {
    return;
  }

  authHandler(appFirestore);
  database.loop();
  docs.loop();

  // Ambil nilai threshold yang disimpan pada Firebase
  if (millis() - lastThresholdUpdate > THRESHOLD_INTERVAL) {
    lastThresholdUpdate = millis();

    //.. Ambil Document dht1 dan dht2 pada Collection 
    //.. threshold yang berisi threshold peringatan buzzer
    Serial.println("Fetching thresholds...");
    String payload = docs.get(clientFirestore, Firestore::Parent(FIREBASE_PROJECT_ID), THRESHOLD_COLLECTION_ID, GetDocumentOptions(DocumentMask()));

    //.. Muat threshold
    json decoded = json::parse(payload);
    json threshold = decoded.at("fields");
    thresholdDHTLeftTemperature = threshold.at("leftTemperature").at("doubleValue").template get<double>();
    thresholdDHTLeftHumidity = threshold.at("leftHumidity").at("doubleValue").template get<double>();
    thresholdDHTRightTemperature = threshold.at("rightTemperature").at("doubleValue").template get<double>();
    thresholdDHTRightHumidity = threshold.at("rightHumidity").at("doubleValue").template get<double>();
    Serial.println(thresholdDHTLeftTemperature);
    Serial.println(thresholdDHTLeftHumidity);
    Serial.println(thresholdDHTRightTemperature);
    Serial.println(thresholdDHTRightHumidity);
    Serial.println(" ");
  }

  if (millis() - lastRealtimeUpdate > MONITORING_INTERVAL) {
    lastRealtimeUpdate = millis();
    // Ambil data suhu dan kelembaban dari dht
    Serial.println("Recording room condition...");
    recordedDHTLeftTemperature = dhtLeft.readTemperature(false);
    recordedDHTLeftHumidity = dhtLeft.readHumidity();
    recordedDHTRightTemperature = dhtRight.readTemperature(false);
    recordedDHTRightHumidity = dhtRight.readHumidity();
    Serial.println(recordedDHTLeftTemperature);
    Serial.println(recordedDHTLeftHumidity);
    Serial.println(recordedDHTRightTemperature);
    Serial.println(recordedDHTRightHumidity);
    Serial.println(" ");

    // Cek apakah suhu dan kelembaban melebihi nilai threshold
    bool leftDanger = recordedDHTLeftHumidity >= thresholdDHTLeftHumidity && recordedDHTLeftTemperature >= thresholdDHTLeftTemperature;
    bool rightDanger =  recordedDHTRightHumidity >= thresholdDHTRightHumidity && recordedDHTRightTemperature >= thresholdDHTRightTemperature;

    if(!leftDanger && !rightDanger) {
      digitalWrite(BUZZER, LOW);
      Serial.println("calm...");
    } else {
      if(leftDanger) {
        digitalWrite(BUZZER, HIGH);
        Serial.println("LEFT, DANGER!!!");
      }
      if(rightDanger) {
        digitalWrite(BUZZER, HIGH);
        Serial.println("RIGHT, DANGER!!!");
      } 
    }
    Serial.println(" ");

    // Simpan data pengukuran ke Realtime Database
    Serial.println("Updating monitor values...");
    Serial.println("Left Humidity...");
    database.set<double>(clientRealtime, "/leftHumidity", recordedDHTLeftHumidity);
    Serial.println("Left Temperature...");
    database.set<double>(clientRealtime, "/leftTemperature", recordedDHTLeftTemperature);
    Serial.println("Right Humidity...");
    database.set<double>(clientRealtime, "/rightHumidity", recordedDHTRightHumidity);
    Serial.println("Right Temperature...");
    database.set<double>(clientRealtime, "/rightTemperature", recordedDHTRightTemperature);
    Serial.println(" ");
  }

  // Simpan rekaman catatan ke Firestore Database
  if (millis() - lastFirestoreUpdate > RECORDS_INTERVAL) {
    lastFirestoreUpdate = millis();
    for(int i = 0; i < 2; i++) {
      //.. Susun jadi 1 dokumen
      Serial.println("Saving recorded conditions...");
      Serial.println("Timestamp...");
      Values::TimestampValue recordTime(getTimestampString(getTime()));
      Serial.println("Temperature...");
      Values::DoubleValue recordTemperature((i == 0) ? recordedDHTLeftTemperature : recordedDHTRightTemperature);
      Serial.println("Humidity...");
      Values::DoubleValue recordHumidity((i == 0) ? recordedDHTLeftHumidity : recordedDHTRightHumidity);
      Serial.println("Type...");
      Values::IntegerValue recordDHTID(i);

      Document<Values::Value> doc("dhtid", Values::Value(recordDHTID));
      doc.add("timestamp", Values::Value(recordTime));
      doc.add("temperature", Values::Value(recordTemperature));
      doc.add("humidity", Values::Value(recordHumidity));

      //.. Buat ID baru untuk dokumen, jadikan payload, dan kirim
      // WARN: Aku belum tahu documentPath to termasuk nama document atau tidak
      // Kalau belum termasuk dan namanya digenerate Firebase, hilangkan documentCount
      // Berdasarkan https://firebase.google.com/docs/firestore/reference/rest/v1/projects.databases.documents/createDocument
      // documentID tu terpisah dari documentPath, jadi harusny ini buat ID acak dewek
      // karena https://github.com/mobizt/FirebaseClient/blob/main/resources/docs/firestore_database.md#-string-createdocumentasyncclientclass-aclient-firestoreparent-parent-const-string-documentpath-documentmask-mask-documentvaluesvalue-document
      String documentPath = RECORDS_COLLECTION_ID;
      Serial.println("Saving...");
      String payload = docs.createDocument(clientFirestore, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc);
      
      //.. Tampilkan payload atau error
      // printPayload(clientFirestore, payload);
      Serial.println(" ");
    }
  }

  // Delay supaya tidak spam 10 milidetik
  // Tidak perlu karena sudah pakai millis
  // delay(1000);
}

// Untuk autentikasi
void authHandler(FirebaseApp app)
{
  // Blocking authentication handler with timeout
  unsigned long ms = millis();
  while (app.isInitialized() && !app.ready() && millis() - ms < 120 * 1000)
  {
    // The JWT token processor required for ServiceAuth and CustomAuth authentications.
    // JWT is a static object of JWTClass and it's not thread safe.
    // In multi-threaded operations (multi-FirebaseApp), you have to define JWTClass for each FirebaseApp,
    // and set it to the FirebaseApp via FirebaseApp::setJWTProcessor(<JWTClass>), before calling initializeApp.
    // Bukan multithreaded, so WHY
    // Litteray disini cuma salahny
    JWT.loop(app.getAuth());
    printResult(resultFirestore);
  }
}

void printPayload(AsyncClient client, String payload)
{
  if (client.lastError().code() == 0) {
    Serial.println("Nggak error");
    Serial.println(payload);
  } else {
    Serial.println("Error wkwwkwkwkw"); 
    Firebase.printf("Error, msg: %s, code: %d\n", client.lastError().message().c_str(), client.lastError().code());
  }
}

void printResult(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}

unsigned long getTime() {
  time_t now;
  tm tm;
  time(&now);
  localtime_r(&now, &tm);
  return now;
}

// Untuk timestamp
String getTimestampString(uint64_t sec)
{
    if (sec > 0x3afff4417f)
        sec = 0x3afff4417f;

    time_t now;
    struct tm ts;
    char buf[80];
    now = sec;
    ts = *localtime(&now);

    String format = "%Y-%m-%dT%H:%M:%SZ";

    strftime(buf, sizeof(buf), format.c_str(), &ts);
    return buf;
}