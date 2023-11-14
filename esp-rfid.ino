#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>

#define RST_PIN 27
#define SS_PIN 5
#define SUCCESS 16
#define ERROR_LED 17
#define MASTER_LED 4
#define RELAY 22
#define CARD_ADDED_SOUND 21
#define INTERNAL_LED 2

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
int MAXTICKS = 30;
unsigned long prevMl = 0;
unsigned long interval = 30000;
const char* ssid = "";
const char* password = ""; // store in flash mem

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
  WiFi.begin(ssid, password);
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
  server.send(200, "text/plain", "It's open, come on in!");
}

void setup() {
  initPins();
  Serial.begin(9600);
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

void success(bool master) {
  digitalWrite(RELAY, HIGH);
  digitalWrite(SUCCESS, HIGH);
  delay(20);

  if (master) {
    digitalWrite(MASTER_LED, LOW);
  } else {
    digitalWrite(CARD_ADDED_SOUND, HIGH);
  }
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
    addedCards++;
    success(false);
  }
}

void flash() {
  delay(250);
  digitalWrite(MASTER_LED, LOW);
  delay(250);
  digitalWrite(MASTER_LED, HIGH);
}

void loop() {
  unsigned long = millis();

  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  digitalWrite(SUCCESS, LOW);
  digitalWrite(MASTER_LED, LOW);
  digitalWrite(ERROR_LED, LOW);
  digitalWrite(RELAY, LOW);
  digitalWrite(CARD_ADDED_SOUND, LOW);

  server.handleClient();

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String card = decode();

  for (int i = 0; i < 5; i++) {
    if (card.substring(1) == masters[i]) {
      digitalWrite(MASTER_LED, HIGH);
      int ticks = 0;

      while (ticks <= MAXTICKS) {
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          String newCard = decode();
          if (!validateMaster(newCard)) return;
          return addCard(newCard);
        }

        flash();
        ticks++;
      }

      return success(true);
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

