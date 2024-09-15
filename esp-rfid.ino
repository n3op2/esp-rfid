#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#define RST_PIN 27
#define SS_PIN 5
#define SUCCESS 16
#define ERROR_LED 17
#define MASTER_LED 4
#define RELAY 22
#define CARD_ADDED_SOUND 21
#define INTERNAL_LED 2

Preferences store;
/*
03 40 F1 AA - 7 
43 71 EA 10 - 8
B9 2E D4 0D - 6
80 F3 83 20 - 9
B3 59 38 0F - edg
*/
String masters[] = { "80 F3 83 20", "43 71 EA 10", "B3 59 38 0F", "03 40 F1 AA", "B9 2E D4 0D" };
String cards[255] = {};  // for multiple cards
int addedCards = 0;
bool master = false;
size_t cardL = sizeof("80 F3 83 20");
int MAXTICKS = 30;
unsigned long prev = 0;
unsigned long interval = 30000;

IPAddress ip(192, 168, 0, 10);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 0, 254);
MFRC522 mfrc522(SS_PIN, RST_PIN);
WebServer server(80);

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
  digitalWrite(SUCCESS, HIGH);
  digitalWrite(RELAY, HIGH);
  server.send(200, "tex\t/plain", "It's open, come on in!");
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
  Serial.println();

  return decoded;
}

void success(bool isNew) {
  if (isNew) {
    digitalWrite(CARD_ADDED_SOUND, HIGH);
    delay(1000);
    digitalWrite(SUCCESS, HIGH);
    return delay(1000);
  }

  digitalWrite(RELAY, HIGH);
  digitalWrite(SUCCESS, HIGH);
  delay(3000);
}

bool validateCard(String card) {
  if (addedCards == 255) {
    addedCards = 0;
  }
  for (int i = 0; i < addedCards; i++) {
    if (card.substring(1) == cards[i]) {
      digitalWrite(ERROR_LED, HIGH);
      delay(2000);
      return false;
    }
  }

  return true;
}

bool validateMaster(String card) {
  for (int i = 0; i < 5; i++) {
    if (card.substring(1) == masters[i]) {
      digitalWrite(ERROR_LED, HIGH);
      delay(2000);

      return false;
    }
  }

  return true;
}

void addCard(String card) {
  if (validateCard(card)) {
    cards[addedCards] = card.substring(1);
    store.putString(String(addedCards).c_str(), card.substring(1));
    addedCards++;
    store.putInt("count", addedCards);
    success(true);
  }
}

void check(String card) {
  for (int i = 0; i < 5; i++) {
    if (card.substring(1) == masters[i]) {
      int ticks = 0;

      while (ticks <= MAXTICKS) {
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          String newCard = decode();
          if (!validateMaster(newCard)) return;
          return addCard(newCard);
        }
        digitalWrite(MASTER_LED, HIGH);
        delay(250);
        digitalWrite(MASTER_LED, LOW);
        delay(250);
        ticks++;
      }

      return success(false);
    }
  }

  for (int i = 0; i < addedCards; i++) {
    if (card.substring(1) == cards[i]) {
      return success(false);
    }
  }

  digitalWrite(ERROR_LED, HIGH);
  delay(1000);
}

void hydrate() {
  unsigned long cur = millis();

  if (cur - prev >= interval) {
    Serial.println("Reconnecting to WiFi...");
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
  
  check(card);
}

