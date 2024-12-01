#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
// TODO: Belum import DHT
 
#define WIFI_SSID "Wifi Rumah" 
#define WIFI_PASSWORD "1992kunci" 
#define FIREBASE_PROJECT_ID "PROJECT_ID"
#define API_KEY "Web_API_KEY"
#define THRESHOLD_COLLECTION_ID "threshold"
#define RECORDS_COLLECTION_ID "records"

// TODO: Belum di cek pin berapa
#define BUZZER 4;
#define DHT_LEFT 4;
#define DHT_RIGHT 4;

// TODO: Belum import DHT
DefaultNetwork network; 
FirebaseApp app;
NoAuth no_auth;
Firestore::Documents docs;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client, getNetwork(network));

double thresholdDHTLeftTemperature;
double thresholdDHTLeftHumidity;
double recordedDHTLeftTemperature;
double recordedDHTLeftHumidity;
double thresholdDHTRightTemperature;
double thresholdDHTRightHumidity;
double recordedDHTRightTemperature;
double recordedDHTRightHumidity;
int documentCount;

void setup() { 
  // Setup perangkat
  Serial.begin(9600); 
  pinMode(BUZZER, OUTPUT);
 
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
  
  // Konek ke Firebase
  //.. Bangun header dan inisialisasi
  ssl_client.setInsecure();
  initializeApp(aClient, app, getAuth(no_auth));
  app.getApp<Firestore::CollectionGroups::Indexes>(indexes);

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
  app.loop();

  //.. TODO: Ambil jumlah record pada Firebase
  if(!documentCount) {
    documentCount = 0;
  }

  // TODO: Ambil data suhu dan kelembaban dari dht
  recordedDHTLeftTemperature = 0;
  recordedDHTLeftHumidity = 0;
  recordedDHTRightTemperature = 0;
  recordedDHTRightHumidity = 0;

  // Ambil nilai threshold yang disimpan pada Firebase
  //.. TODO: Ambil Document dht1 dan dht2 pada Collection 
  //.. threshold yang berisi threshold peringatan buzzer
  String payload = docs.list(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), THRESHOLD_COLLECTION_ID);

  //.. Tampilkan payload atau error
  if (aClient.lastError().code() == 0)
    Serial.println(payload);
  else
    printError(aClient.lastError().code(), aClient.lastError().message());

  //.. TODO: Muat threshold
  thresholdDHTLeftTemperature = 0;
  thresholdDHTLeftHumidity = 0;
  thresholdDHTRightTemperature = 0;
  thresholdDHTRightHumidity = 0;

  // Cek apakah suhu dan kelembaban melebihi nilai threshold
  if((recordedDHTLeftHumidity >= thresholdDHTLeftHumidity && recordedDHTLeftTemperature >= thresholdDHTLeftTemperature) ||
    (recordedDHTRightHumidity >= thresholdDHTRightHumidity && recordedDHTRightTemperature >= thresholdDHTRightTemperature)) {
    digitalWrite(BUZZER, HIGH);
  } else {
    digitalWrite(BUZZER, LOW);
  }

  for(int i = 0; i < 2; i++) {
    // Simpan rekamanan catatan ke Firebase
    //.. TODO: Susun jadi 1 dokumen
    Values::TimestampValue recordTime(getTimestampString(1712674441, 999999999));
    Values::DoubleValue recordTemperature((i == 0) ? recordedDHTLeftTemperature : recordedDHTRightTemperature);
    Values::DoubleValue recordHumidity((i == 0) ? recordedDHTLeftHumidity : recordedDHTRightHumidity);
    Values::IntegerValue recordDHTID(i);
    Document<Values::Value> doc("dhtid", Values::Value(recordDHTID));
    doc.add("time", Values::Value(recordTime));
    doc.add("temperature", Values::Value(recordTemperature));
    doc.add("humidity", Values::Value(recordHumidity));

    //.. Buat ID baru untuk dokumen dan jadikan payload
    String documentPath = RECORDS_COLLECTION_ID + String(documentCount);
    String payload = docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc);
    
    //.. Tampilkan payload atau error
    if (aClient.lastError().code() == 0)
      Serial.println(payload);
    else
      printError(aClient.lastError().code(), aClient.lastError().message());

    documentCount++;
  }

  // Ulang setiap 5 detik
  delay(5000);
}