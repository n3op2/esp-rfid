#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFiClient.h>


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
String ssid = "hatushka";
String password = "aaaaaaaa";
int addedCards = 0;
bool master = false;
size_t cardL = sizeof("80 F3 83 20");
int MAXTICKS = 30;
unsigned long prev = 0;
unsigned long interval = 30000;

IPAddress ip(192, 168, 0, 10);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 0, 1);
MFRC522 mfrc522(SS_PIN, RST_PIN);
WebServer server(80);
HTTPClient http;
WiFiClient client;

void initPins() {
  pinMode(ERROR_LED, OUTPUT);
  pinMode(MASTER_LED, OUTPUT);
  pinMode(SUCCESS, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(CARD_ADDED_SOUND, OUTPUT);
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid.c_str(), password.c_str());
  int retries = 10;

  while (retries > 0) {
      delay(500);
      retries--;
  }
  Serial.println(WiFi.localIP());
}

void handleOpen() {
  success();
  server.send(200, "tex\t/plain", "It's open, come on in!");
}

void handleAdd() {
  server.send(200, "text\t/plain", "will add a card");
}

void handleList() {
  digitalWrite(MASTER_LED, HIGH);
  delay(50);
  String list = "";
  
  
  if (addedCards > 0) {
    for (int i = 0; i < addedCards; i++) {
      list = list + "\n" + cards[i];
    }
  }
  server.send(200, "tex\t/plain", list);
}

void setup() {
  initPins();
  Serial.begin(115200);
  store.begin("rfid", false);
  addedCards = store.getInt("count", 0);
  if (addedCards > 0) {
    for (int i = 0; i < addedCards; i++) {
      String card;
      card = store.getString(String(i).c_str(), ""); 
      cards[i] = card;
    }
  }
    Serial.end();
    Serial.begin(9600);

  store.putString("ssid", ssid); 
  store.putString("password", password);
  delay(10);
  SPI.begin();
  mfrc522.PCD_Init();  // Initiate MFRC522
  connectWifi();
  server.on("/open", handleOpen);
  server.on("/card/add", handleAdd);
  server.on("/card/list", handleList);
  server.begin();
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
  for (int i = 0; i < addedCards; i++) {
    if (card == cards[i]) {
      digitalWrite(ERROR_LED, HIGH);
      delay(2000);
      return false;
    }
  }

  return true;
}

bool validateMaster(String card) {
  for (int i = 0; i < 5; i++) {
    if (card == masters[i]) {
      digitalWrite(ERROR_LED, HIGH);
      delay(2000);

      return false;
    }
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

void hydrate() {
  unsigned long cur = millis();

  if (cur - prev >= interval) {
    WiFi.disconnect();
    WiFi.reconnect();
    prev = cur;
  }

  digitalWrite(SUCCESS, LOW);
  digitalWrite(MASTER_LED, LOW);
  digitalWrite(ERROR_LED, LOW);
  digitalWrite(RELAY, LOW);
  digitalWrite(CARD_ADDED_SOUND, LOW);

}


void loop() {
  hydrate();
  server.handleClient();

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
