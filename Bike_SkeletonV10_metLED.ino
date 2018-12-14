
// Main setup:
#include <TimeLib.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "SPI.h"
#include "PN532_SPI.h"
#include "snep.h"
#include "NdefMessage.h"
#include <SHA256.h>

#define HASH_SIZE 32
#define BLOCK_SIZE 64

static const int RXPin = 2, TXPin = 3;
static const uint32_t GPSBaud = 9600;
static const int lockLed = 7;
static int locked = 0;

TinyGPSPlus gps;

// NFC setup:
char firstCharTimestamp;
boolean firstPush;
boolean messageRecieved;
char payloads[4][32];
int payloadLengths[4];
char bike_key[32] = "LRFKQYUQFJKXYQVNRTYSFRZRMZLYGFVE";
PN532_SPI pn532spi(SPI, 10);
SNEP nfc(pn532spi);
char HASH[65];
uint8_t ndefBuf[128];
int timeInt = 0;

// GPS setup:
SoftwareSerial ss(2, 3);
SoftwareSerial hc(4, 5);


void setup() {

  Serial.begin(115200);
  ss.begin(9600);
  hc.begin(9600);
  pinMode(13, OUTPUT);
  messageRecieved = false;
  firstPush = false;
  timeInt = 42;
  timeInt = get_timestamp().toInt();

}

void loop() {
  printFreeRam();
  updater(false);
  NFC_read();
  if (messageRecieved)
  {
    messageRecieved = false;

    if (!checkHash()) {

      Serial.println(F("secrets did not match"));
    } else {
      Serial.println(F("secrets match"));
      Serial.println(firstCharTimestamp);

      if (firstCharTimestamp == '0') {
        encContract();
      } else {
        toggleLock();
      }
    }
  }
}

void updater(bool secretBool) {
  hc.listen();
  Serial.println(F("UPDATER"));


  if (hc.available() > 0) {
    Serial.println(F("Waiting for GPS update..."));
    String secret;
    String coords = get_coords();
    hc.listen();
    hc.print(coords);
    hc.print(";");
    Serial.println(coords);
    Serial.println(F("Generating new secret..."));
    if (secretBool || firstPush) {
      Serial.println("blabla");
      generateSecret(timeInt);
      secret = String(bike_key);
      Serial.println(bike_key);
      firstPush = false;
    } else {
      Serial.println("Secret == False");
      secret.remove(0);
      secret = String(1);
      Serial.println(bike_key);

      //      Serial.println(secret);
    }
    hc.listen();
    hc.print(secret);
    hc.print(";");
    Serial.println(secret);
    String timestamp = get_timestamp();
    timeInt = timestamp.toInt();
    hc.print(timestamp);
    Serial.println(timestamp);
  }
}


void generateSecret(int times) {
    randomSeed(times);

  for (int i = 0; i < 32; i++) {

    int n = random(65, 91);
    bike_key[i] = char(n);


  }
  bike_key[32] = '\0';
}

// Lock (hardware) functions
void change_lock_state(boolean lock) {
  // Changes the lock from locked to unlocked or from unlocked to locked.
  // Also turns the green and red LED on or off
  // And Pin 13 (Buildin LED) on or off
  // On -> Locked, Off -> Unlocked
  // No output
  digitalWrite(13, lock);
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

// Data sending functions
String get_coords() {
  ss.begin(9600);
  smartDelay(2000);
  return String(gps.location.lat(), 6) + ";" + String(gps.location.lng(), 6);
}

String get_timestamp() {
  ss.begin(9600);
  smartDelay(1000);

  setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
  //Serial.println(now());
  //  time_t t = now();
  return String(now());
}

void still_alive() {
  //Send something to server to verify this arduino and bike are still alive
}



// Data recieving functions
void NFC_read() {
  printFreeRam();

  // Reads incoming NFC signal and returns that signal
  //delay(1000);
  //Serial.println(int(ndefBuf));
  Serial.println(F("Waiting for message from a peer"));
  int msgSize = nfc.read(ndefBuf, sizeof(ndefBuf), 10000);
  if (msgSize == 0) {
    Serial.println(F("PROBLEMEN"));
    msgSize = -1;

  }
  Serial.println(msgSize);
  if (msgSize > 0) {
    //printFreeRam();
    NdefMessage msg  = NdefMessage(ndefBuf, msgSize);
    msg.print();

    int n = msg.getRecordCount();



    for (int i = 0; i < n; i++) {


      NdefRecord record = msg.getRecord(i);

      int payloadLength = record.getPayloadLength();
      byte payload[payloadLength];
      record.getPayload(payload);

      // The TNF and Type are used to determine how your application processes the payload
      // There's no generic processing for the payload, it's returned as a byte[]
      int startChar = 0;
      if (record.getTnf() == TNF_WELL_KNOWN && record.getType() == "T") { // text message
        // skip the language code
        startChar = payload[0] + 1;
      } else if (record.getTnf() == TNF_WELL_KNOWN && record.getType() == "U") { // URI
        // skip the url prefix (future versions should decode)
        startChar = 1;
      }

      // Force the data into a String (might fail for some content)
      // Real code should use smarter processing

      for (int c = startChar; c < payloadLength; c++) {
        payloads[i][c] = (char)payload[c];
      }
      payloads[i][payloadLength] =  '\0';
      payloadLengths[i] = payloadLength;
      Serial.println(payloads[i]);
    }
    Serial.println(F("##############"));

    for (int i = 0; i < 4; i++) {
      Serial.println(payloads[i]);
    }

    messageRecieved = true;

  }
}

boolean checkHash() {
  Serial.println(F("PUTTING VARIABLES"));

  //char* hash2User = (char*)malloc(payloadLengths[0]+1);
  char hash2User[payloadLengths[0] + 1];
  strncpy ( hash2User, payloads[0], payloadLengths[0] );
  hash2User[payloadLengths[0]] =  '\0';
  Serial.println(hash2User);

  //  char* usernameshort = (char*)malloc(payloadLengths[3]+1);
  char usernameshort[payloadLengths[3] + 1];
  strncpy ( usernameshort, payloads[3], payloadLengths[3] );
  usernameshort[payloadLengths[3]] =  '\0';
  Serial.println(usernameshort);


  //char* startDshort = (char*)malloc(payloadLengths[2]+1);
  char startDshort[payloadLengths[2] + 1];
  strncpy ( startDshort, payloads[2], payloadLengths[2] );
  startDshort[payloadLengths[2]] =  '\0';
  Serial.println(startDshort);


  //  timestampshort = (char*)malloc(payloadLengths[1]+1);
  char timestampshort[payloadLengths[1] + 1];
  strncpy( timestampshort, payloads[1], payloadLengths[1] );
  timestampshort[payloadLengths[1]] =  '\0';
  Serial.println(timestampshort);
  firstCharTimestamp = timestampshort[0];


  Serial.println(F("Start HASHING"));
  printFreeRam();

  Serial.println(F("CHECKING TIMESTAMP"));
  Serial.println(firstCharTimestamp);
  //Serial.println(memcmp(firstCharTimestamp, '0', 5));

  if (firstCharTimestamp == '0') {
    Serial.println(F("TIMESTAMP = 0"));
  } else {
    //int timestamp = atoi(timestampshort);
    int NewTime = get_timestamp().toInt();
    //   Serial.println(get_timestamp());


    if (NewTime != timeInt){
      NewTime = timeInt;
      if (String(timestampshort).toInt() < timeInt - 60) {
        Serial.println(F("NIET IN ORDE"));

      return false;
    }
    }

  }


  Serial.println(F("CONSTRUCTING HASH2DATA"));

  char hash1Data[32 + payloadLengths[2] + payloadLengths[3] + 1];
  sprintf(hash1Data, "%s%s%s", bike_key, startDshort, usernameshort);


  Serial.println(F("HASH1"));
  //char hash1[65];
  printFreeRam();
  HashData(hash1Data, 64);
  //delay(500);
  printFreeRam();

  Serial.println(F("HASH1 +DATA"));
  Serial.println(HASH);
  Serial.println(hash1Data);

  // free(hash1Data);
  Serial.println(F("CONSTRUCTING HASH2DATA"));

  char hash2Data[strlen(HASH) + strlen(timestampshort) + 1];
  sprintf(hash2Data, "%s%s", timestampshort, HASH);


  //delete[] hash2Data;
  //free(hash2Data);


  Serial.println(F("HASH2"));
  HashData2(hash2Data, 1);

  Serial.println(F("FINAL HASH + HASHINPUT"));
  Serial.println(HASH);
  Serial.println(hash2Data);
  printFreeRam();

  Serial.println(F("############"));
  Serial.println(hash2User);
  Serial.println(F("############"));

  if (memcmp(hash2User, HASH, 32) == 0) {
    return true;
  } else {
    return false;
  }
}

void HashData( char* test, size_t inc)
{
  Serial.println(F("######inhashing######"));

  Serial.println(test);
  Serial.println(strlen(test));

  size_t size = strlen(test);
  size_t posn, len;
  uint8_t value[HASH_SIZE];
  SHA256 sha256;
  Hash *hash = &sha256;
  hash->reset();

  for (posn = 0; posn < size; posn += inc) {
    len = size - posn;
    if (len > inc)
      len = inc;
    hash->update(test + posn, len);
  }
  hash->finalize(value, sizeof(value));

  char hex[2];
  for (int i = 0; i < 32; i++) {
    sprintf(hex, "%x", value[i]);

    //  Serial.println(value[i]); // LATEN STAAN!!!!

    if (strlen(hex) < 2)
    { HASH[i * 2] = '0';
      HASH[(i * 2) + 1] = toupper(hex[0]);


    } else {
      HASH[i * 2] = toupper(hex[0]);
      HASH[(i * 2) + 1] = toupper(hex[1]);

    }
  }
  HASH[64] = '\0';

  Serial.println(strlen(HASH));
  Serial.println(F("######endhashing######"));
  hash->reset();
}

void HashData2( char* test, size_t inc)
{
  Serial.println(F("######inhashing######"));

  Serial.println(test);
  Serial.println(strlen(test));

  size_t size = strlen(test);
  size_t posn, len;
  uint8_t value[HASH_SIZE];
  SHA256 sha256;
  Hash *hash = &sha256;
  hash->reset();

  for (posn = 0; posn < size; posn += inc) {
    len = size - posn;
    if (len > inc)
      len = inc;
    hash->update(test + posn, len);
  }
  hash->finalize(value, sizeof(value));

  char hex[2];
  for (int i = 0; i < 32; i++) {
    sprintf(hex, "%x", value[i]);

    //  Serial.println(value[i]); // LATEN STAAN!!!!

    if (strlen(hex) < 2)
    { HASH[i * 2] = '0';
      HASH[(i * 2) + 1] = toupper(hex[0]);


    } else {
      HASH[i * 2] = toupper(hex[0]);
      HASH[(i * 2) + 1] = toupper(hex[1]);

    }
  }
  HASH[64] = '\0';

  Serial.println(strlen(HASH));
  Serial.println(F("######endhashing######"));
  hash->reset();
}


void encContract() {
  Serial.println(F("Ending contract .."));
  closeLock();
  updater(true);
}


void toggleLock() {
  Serial.println(F("Toggle Lock"));
  locked = (locked - 1) * (locked - 1);
  digitalWrite(lockLed, locked);
}


void closeLock() {
  Serial.println(F("Close Lock"));
  locked = 0;
  digitalWrite(lockLed, locked);
}
/*
  char* RecordToChar(NdefRecord record)
  {
  int payloadLength = record.getPayloadLength();
  byte payload[payloadLength];
  record.getPayload(payload);
  uint32_t szPos;
  char* payloadAsString = "";



  for (szPos = 0; szPos < payloadLength; szPos++)
  {

    payloadAsString[szPos] += (char)payload[szPos];
    //  Serial.print((char)data[szPos]);}
  }
  //Serial.println(payloadAsString);
  return payloadAsString;
  }*/
/*
  String HexChar(byte * data,long numBytes)
  {
  uint32_t szPos;
  String payloadAsString = "";
  payloadAsString.reserve(numBytes);

  for (szPos=0; szPos < numBytes; szPos++)
  {

      payloadAsString[szPos] += (char)data[szPos];
    //  Serial.print((char)data[szPos]);}
  }
  //Serial.println(payloadAsString);
  return payloadAsString;
  }
*/
void printFreeRam() {
  Serial.print(F("Free Memory: "));
  Serial.print(freeRam());
  Serial.println(F(" Bytes"));
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// Security functions
//void check_key(bike_key){
// Checks if the recieved key/hash/password/... is the correct one
// Outputs 0 or 1
//}

//void change_key(new_key){
// Changes the old key to a new one
//}
