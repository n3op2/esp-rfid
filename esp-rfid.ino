#include <MFRC522.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>

#define RST_PIN 27
#define SS_PIN 5
#define SUCCESS 16
#define ERROR_LED 17
#define MASTER_LED 4
#define RELAY 22
#define CARD_ADDED_SOUND 21
#define INTERNAL_LED 2

Preferences store;
String masters[] = { "80 F3 83 20", "43 71 EA 10", "B3 59 38 0F", "03 40 F1 AA", "B9 2E D4 0D", "E3 10 D4 F8", "E3 74 CB F5", "05 17 C6 AE", "33 19 43 F6"};
String cards[255] = {};  // for multiple cards
const char* ssid = "hatushka";
const char* password = "aaaaaaaa";
int addedCards = 0;
bool master = false;
int MAXTICKS = 30;

MFRC522 mfrc522(SS_PIN, RST_PIN);
WebServer server(IPAddress(0,0,0,0), 80);
HTTPClient http;

WiFiMulti multi;
WiFiClient client;
unsigned long WIFI_MULTI_RUN_TIMEOUT_MS = 30000;

void setupPins() {
  pinMode(ERROR_LED, OUTPUT);
  pinMode(MASTER_LED, OUTPUT);
  pinMode(SUCCESS, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(CARD_ADDED_SOUND, OUTPUT);
}

void handleOpen() {
  success();
  delay(1000);
  server.send(200, "tex\t/plain", "It's open, come on in!");
}

void handleAdd() {
  server.send(200, "text\t/plain", "will add a card");
}

void handleList() {
  digitalWrite(MASTER_LED, HIGH);
  delay(50);
  String list = "";
  for (int i = 0; i < addedCards; i++) {
    list = list + cards[i] + "\n";
  }
  server.send(200, "text/plain", list);
}

void wifiReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    if (multi.run(WIFI_MULTI_RUN_TIMEOUT_MS) == WL_CONNECTED) {
        Serial.print("Successfully connected to network: ");
        Serial.println(WiFi.localIP());
        server.begin();
    } 
  }
}

void wifiSetup() {
  multi.addAP(ssid, password);
  wifiReconnect();
}

String decode() {
  String decoded = "";

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    decoded.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    decoded.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  decoded.toUpperCase();
  Serial.print(decoded.substring(1));

  return decoded.substring(1);
}

void success() {
  digitalWrite(RELAY, HIGH);
  digitalWrite(CARD_ADDED_SOUND, HIGH);
  digitalWrite(SUCCESS, HIGH);
  delay(3000);
  if (http.begin(client, "http://192.168.0.11/success")) {
    http.end();
  }
}

bool validateCard(String card) {
  if (addedCards == 255) {
    addedCards = 0;
  }
  if (isAddedCard(card)) {
    digitalWrite(ERROR_LED, HIGH);
    delay(2000);
    return false;
  }
  return true;
}

bool isAddedCard(String card) {
  for (int i = 0; i < addedCards; i++) {
    if (card == cards[i]) {
      return true;
    }
  }

  return false;
}

bool isMaster(String card) {
  for (int i = 0; i < 5; i++) {
    if (card == masters[i]) {
      return true;
    }
  }

  return false;  
}

bool validateMaster(String card) {
  if (isMaster(card)) {
    digitalWrite(ERROR_LED, HIGH);
    delay(2000);
    return false;
  }
  return true;
}

void addCard(String card) {
  if (validateCard(card)) {
    cards[addedCards] = card;
    store.putString(String(addedCards).c_str(), card);
    addedCards++;
    store.putInt("count", addedCards);
    success();
  }
}

void check(String card) {
  for (int i = 0; i < 9; i++) {
    if (card == masters[i]) {
      int ticks = 0;

      while (ticks <= MAXTICKS) {
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          String newCard = decode();
          if (!validateMaster(newCard)) return;
          return addCard(newCard);
        }
        digitalWrite(MASTER_LED, HIGH);
        delay(450);
        digitalWrite(MASTER_LED, LOW);
        delay(50);
        ticks++;
      }

      return success();
    }
  }

  for (int i = 0; i < addedCards; i++) {
    if (card == cards[i]) {
      return success();
    }
  }

  digitalWrite(ERROR_LED, HIGH);
  delay(1000);
}

void digitalWriteAllLow() {
  digitalWrite(SUCCESS, LOW);
  digitalWrite(MASTER_LED, LOW);
  digitalWrite(ERROR_LED, LOW);
  digitalWrite(RELAY, LOW);
  digitalWrite(CARD_ADDED_SOUND, LOW);
}

void setup() {
  Serial.begin(115200);
  setupPins();
  server.on("/open", handleOpen);
  server.on("/card/add", handleAdd);
  server.on("/card/list", handleList);
  wifiSetup();
  store.begin("rfid", false);
  addedCards = store.getInt("count", 0);

  for (int i = 0; i < addedCards; i++) {
    String card;
    card = store.getString(String(i).c_str(), ""); 
    cards[i] = card;
  }

  delay(10);
  SPI.begin();
  mfrc522.PCD_Init();  // Initiate MFRC522
 
}


void loop() {
  wifiReconnect();
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  digitalWriteAllLow();

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String card = decode();
  Serial.println(card);
  
  check(card);
}
