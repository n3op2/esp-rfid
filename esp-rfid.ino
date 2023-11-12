#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 0
#define SS_PIN 5
#define SUCCESS 16  // relay ...
#define ERROR_LED 2
#define MASTER_LED 32
#define RELAY 17
#define SOUND 25

MFRC522 mfrc522(SS_PIN, RST_PIN);

String masters[] = {}; // define master cards here 
String cards[255] = {};  // for multiple cards
int addedCards = 0;
bool master = false;
int MAXTICKS = 20;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();  // Initiate MFRC522
  pinMode(ERROR_LED, OUTPUT);
  pinMode(MASTER_LED, OUTPUT);
  pinMode(SUCCESS, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(SOUND, OUTPUT);
}

String decode() {
  String decoded = "";

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    decoded.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    decoded.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  decoded.toUpperCase();
  Serial.print(decoded);
  Serial.println();

  return decoded;
}

void success(bool master) {
  digitalWrite(SUCCESS, HIGH);
  digitalWrite(RELAY, HIGH);

  if (master) {
    digitalWrite(MASTER_LED, LOW);
  } else {
    digitalWrite(SOUND, HIGH);
  }
  delay(3000);
}

bool validateCard(String card) {
  if (addedCards == 255) {
    addedCards = 0;
  }return false;
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
      digitalWrite(SOUND, HIGH);
  delay(250);
  digitalWrite(MASTER_LED, LOW);
  delay(250);
  digitalWrite(MASTER_LED, HIGH);
}

void loop() {
  digitalWrite(SUCCESS, LOW);
  digitalWrite(MASTER_LED, HIGH);
    digitalWrite(MASTER_LED, HIGH);
  digitalWrite(ERROR_LED, LOW);
  digitalWrite(RELAY, LOW);
  digitalWrite(SOUND, LOW);

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
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
