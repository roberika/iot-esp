#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
// #include "DHT.h"
#include "time.h"
 
#define WIFI_SSID "Wifi Rumah" 
#define WIFI_PASSWORD "1992kunci" 
#define DHT_TYPE DHT11  
#define FIREBASE_PROJECT_ID "dht-firebase-if51"
#define API_KEY "AIzaSyDwOZzT_L0wlNby5OrAscTdL8sBKDmZ1ik"
#define THRESHOLD_COLLECTION_ID "threshold/dht1"
#define RECORDS_COLLECTION_ID "records"
#define RECORDS_INTERVAL 10000
// #define USER_EMAIL "dht-firebase@if51.mdp.ac.id"
// #define USER_PASSWORD "janganlupo"
#define USER_EMAIL "robatononihon@gmail.com"
#define USER_PASSWORD "woweee"
#define DATABASE_URL "https://dht-firebase-if51-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DATABASE_SECRET "seXLmFU5gfHCwQ8SE4Y9yK1iDxLv1CAMJZtAkTed"
#define MONITORING_INTERVAL 1000
void authHandler(FirebaseApp appFirestore);
String getTimestampString(uint64_t sec);
unsigned long getTime();
void printResult(AsyncResult &aResult);

// TODO: Belum di cek pin berapa
#define BUZZER 4;
#define DHT_LEFT 4;
#define DHT_RIGHT 4;

// DHT dhtLeft(DHT_LEFT, DHT_TYPE);
// DHT dhtRight(DHT_RIGHT, DHT_TYPE);

// TODO: Having auth problems, switching to ServiceAuth tommorow
DefaultNetwork network; 
FirebaseApp appFirestore, appRealtime;
UserAuth userAuth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
LegacyToken legacyToken(DATABASE_SECRET);
RealtimeDatabase database;
AsyncResult resultFirestore, resultRealtime;
Firestore::Documents docs;
WiFiClientSecure sslFirestore, sslRealtime;
AsyncClientClass clientFirestore(sslFirestore, getNetwork(network)), clientRealtime(sslRealtime, getNetwork(network));

double thresholdDHTLeftTemperature;
double thresholdDHTLeftHumidity;
double recordedDHTLeftTemperature;
double recordedDHTLeftHumidity;
double thresholdDHTRightTemperature;
double thresholdDHTRightHumidity;
double recordedDHTRightTemperature;
double recordedDHTRightHumidity;
unsigned long lastFirestoreUpdate = 0;
unsigned long lastRealtimeUpdate = 0;
unsigned long epochTime; 
const char* ntpServer = "pool.ntp.org";

void setup() { 
  // Setup perangkat
  Serial.begin(115200); 
  Serial.println(" ");
  // pinMode(BUZZER, OUTPUT);
  // dhtLeft.begin();
  // dhtRight.begin();
 
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
  configTime(0, 3600*7, ntpServer);
  epochTime = getTime();
  
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
  // Serial.println("New loop new me");
  Serial.println("Auth Firestore");
  authHandler(appFirestore);
  Serial.println("Loop database");
  database.loop();
  Serial.println("Loop docs");
  docs.loop();
  // Serial.println("Kenja time");

  // Ambil data suhu dan kelembaban dari dht
  recordedDHTLeftTemperature = 40;
  recordedDHTLeftHumidity = 34;
  recordedDHTRightTemperature = 35;
  recordedDHTRightHumidity = 36;
  // recordedDHTLeftTemperature = dhtLeft.readTemperature(false);
  // recordedDHTLeftHumidity = dhtLeft.readHumidity();
  // recordedDHTRightTemperature = dhtRight.readTemperature(false);
  // recordedDHTRightHumidity = dhtRight.readHumidity();

  // Ambil nilai threshold yang disimpan pada Firebase
  //.. Ambil Document dht1 dan dht2 pada Collection 
  //.. threshold yang berisi threshold peringatan buzzer
  String payload = docs.get(clientFirestore, Firestore::Parent(FIREBASE_PROJECT_ID), THRESHOLD_COLLECTION_ID, GetDocumentOptions(DocumentMask()));

  //.. Tampilkan payload atau error
  Serial.println("Fetching...");
  if (clientFirestore.lastError().code() == 0) {
    Serial.println("Nggak error");
    Serial.println(payload);
  } else {
    Serial.println("Error wkwwkwkwkw");
    Firebase.printf("Error, msg: %s, code: %d\n", clientFirestore.lastError().message().c_str(), clientFirestore.lastError().code());
  }
  printResult(resultFirestore);
  Serial.println(" ");
  // yield();

  //.. TODO: Muat threshold
  thresholdDHTLeftTemperature = 30;
  thresholdDHTLeftHumidity = 30;
  thresholdDHTRightTemperature = 30;
  thresholdDHTRightHumidity = 30;

  // Cek apakah suhu dan kelembaban melebihi nilai threshold
  // if((recordedDHTLeftHumidity >= thresholdDHTLeftHumidity && recordedDHTLeftTemperature >= thresholdDHTLeftTemperature) ||
  //   (recordedDHTRightHumidity >= thresholdDHTRightHumidity && recordedDHTRightTemperature >= thresholdDHTRightTemperature)) {
  //   digitalWrite(BUZZER, HIGH);
  // } else {
  //   digitalWrite(BUZZER, LOW);
  // }

  // This works fine
  // Simpan rekaman catatan ke Realtime Database
  if (millis() - lastRealtimeUpdate > MONITORING_INTERVAL) {
    lastRealtimeUpdate = millis();
    Serial.println("Updating...");
    Serial.println("Humidity Left...");
    database.set<double>(clientRealtime, "/leftHumidity", recordedDHTLeftHumidity);
    Serial.println("Temperature Left...");
    database.set<double>(clientRealtime, "/leftTemperature", recordedDHTLeftTemperature);
    Serial.println("Humidity Right...");
    database.set<double>(clientRealtime, "/rightHumidity", recordedDHTRightHumidity);
    Serial.println("Temperature Right...");
    database.set<double>(clientRealtime, "/rightTemperature", recordedDHTRightTemperature);
    Serial.println(" ");
  }

  // Simpan rekamanan catatan ke Firestore Database
  if (millis() - lastFirestoreUpdate > RECORDS_INTERVAL) {
    lastFirestoreUpdate = millis();
    for(int i = 0; i < 2; i++) {
      //.. Susun jadi 1 dokumen
      Serial.println("Time...");
      Values::TimestampValue recordTime(getTimestampString(getTime()));
      Serial.println("Temperature...");
      Values::DoubleValue recordTemperature((i == 0) ? recordedDHTLeftTemperature : recordedDHTRightTemperature);
      Serial.println("Humidity...");
      Values::DoubleValue recordHumidity((i == 0) ? recordedDHTLeftHumidity : recordedDHTRightHumidity);
      Serial.println("Type...");
      Values::IntegerValue recordDHTID(i);
      Serial.println("Combined...");
      Document<Values::Value> doc("dhtid", Values::Value(recordDHTID));
      doc.add("time", Values::Value(recordTime));
      doc.add("temperature", Values::Value(recordTemperature));
      doc.add("humidity", Values::Value(recordHumidity));

      //.. Buat ID baru untuk dokumen, jadikan payload, dan kirim
      // WARN: Aku belum tahu documentPath to termasuk nama document atau tidak
      // Kalau belum termasuk dan namanya digenerate Firebase, hilangkan documentCount
      // Berdasarkan https://firebase.google.com/docs/firestore/reference/rest/v1/projects.databases.documents/createDocument
      // documentID tu terpisah dari documentPath, jadi harusny ini buat ID acak dewek
      // karena https://github.com/mobizt/FirebaseClient/blob/main/resources/docs/firestore_database.md#-string-createdocumentasyncclientclass-aclient-firestoreparent-parent-const-string-documentpath-documentmask-mask-documentvaluesvalue-document
      String documentPath = RECORDS_COLLECTION_ID;
      String payload = docs.createDocument(clientFirestore, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc);
      //.. Tampilkan payload atau error
      Serial.println("Saving...");
      if (clientFirestore.lastError().code() == 0) {
        Serial.println("Nggak error");
        Serial.println(payload);
      } else {
        Serial.println("Error wkwwkwkwkw"); 
        Firebase.printf("Error, msg: %s, code: %d\n", clientFirestore.lastError().message().c_str(), clientFirestore.lastError().code());
      }
      documentCount++;
      // yield();
      Serial.println(" ");
    }
  }

  // Delay supaya tidak spam 10 milidetik
  // Serial.println("I'm going to delay");
  delay(1000);
  // Serial.println("I delayed");
}

// Untuk autentikasi
void authHandler(FirebaseApp appFirestore)
{
  // Blocking authentication handler with timeout
  unsigned long ms = millis();
  while (appFirestore.isInitialized() && !appFirestore.ready() && millis() - ms < 120 * 1000)
  {
    // The JWT token processor required for ServiceAuth and CustomAuth authentications.
    // JWT is a static object of JWTClass and it's not thread safe.
    // In multi-threaded operations (multi-FirebaseApp), you have to define JWTClass for each FirebaseApp,
    // and set it to the FirebaseApp via FirebaseApp::setJWTProcessor(<JWTClass>), before calling initializeApp.
    // Bukan multithreaded, so WHY
    // Litteray disini cuma salahny
    JWT.loop(appFirestore.getAuth());
    printResult(resultFirestore);
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
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
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