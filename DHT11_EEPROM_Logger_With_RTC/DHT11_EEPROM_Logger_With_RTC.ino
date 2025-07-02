it#include <DHT.h>
#include <EEPROM.h>

// DS1302 pins
#define DS1302_CLK 4
#define DS1302_DAT 3
#define DS1302_RST 5

#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// Structure des données enregistrées
struct Record {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  int8_t temperature;
};

const int EEPROM_START = 0;
const int RECORD_SIZE = sizeof(Record);
const int MAX_RECORDS = EEPROM.length() / RECORD_SIZE;

// BCD ⇄ Decimal
uint8_t bcd2dec(uint8_t b) { return ((b >> 4) * 10 + (b & 0x0F)); }
uint8_t dec2bcd(uint8_t d) { return (((d / 10) << 4) | (d % 10)); }

// Fonctions DS1302 bas niveau
void ds1302_start() {
  pinMode(DS1302_RST, OUTPUT);
  digitalWrite(DS1302_RST, LOW);
  pinMode(DS1302_CLK, OUTPUT);
  pinMode(DS1302_DAT, OUTPUT);
  digitalWrite(DS1302_CLK, LOW);
}

void ds1302_writeByte(uint8_t val) {
  pinMode(DS1302_DAT, OUTPUT);
  for (int i = 0; i < 8; i++) {
    digitalWrite(DS1302_DAT, val & 0x01);
    digitalWrite(DS1302_CLK, HIGH);
    digitalWrite(DS1302_CLK, LOW);
    val >>= 1;
  }
}

uint8_t ds1302_readByte() {
  uint8_t val = 0;
  pinMode(DS1302_DAT, INPUT);
  for (int i = 0; i < 8; i++) {
    val >>= 1;
    if (digitalRead(DS1302_DAT))
      val |= 0x80;
    digitalWrite(DS1302_CLK, HIGH);
    digitalWrite(DS1302_CLK, LOW);
  }
  return val;
}

void writeRegister(uint8_t reg, uint8_t val) {
  ds1302_start();
  digitalWrite(DS1302_RST, HIGH);
  ds1302_writeByte((reg << 1) | 0x80);
  ds1302_writeByte(val);
  digitalWrite(DS1302_RST, LOW);
}

uint8_t readRegister(uint8_t reg) {
  ds1302_start();
  digitalWrite(DS1302_RST, HIGH);
  ds1302_writeByte((reg << 1) | 0x81);
  uint8_t val = ds1302_readByte();
  digitalWrite(DS1302_RST, LOW);
  return val;
}

void rtc_halt(bool stop) {
  uint8_t sec = readRegister(0); // Seconds register
  if (stop)
    sec |= 0x80;
  else
    sec &= ~0x80;
  writeRegister(0, sec);
}

// Lire l’heure et la date
void readTime(uint8_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute) {
  year = bcd2dec(readRegister(6));
  month = bcd2dec(readRegister(4));
  day = bcd2dec(readRegister(3));
  hour = bcd2dec(readRegister(2)) - 1;
  minute = bcd2dec(readRegister(1));
}

uint8_t readSeconds() {
  return bcd2dec(readRegister(0) & 0x7F); // Mask halt bit (bit7)
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  rtc_halt(false); // Démarrer RTC si arrêtée
  writeRegister(7, 0x00); // write protect = off

  Serial.println("System ready. Tapez 'r' pour lire l'historique.");
}

// Timers
static unsigned long lastLogMillis = 0;
static unsigned long lastPrintMillis = 0;
static unsigned long lastStatsMeasureMillis = 0;
static unsigned long windowStartMillis = 0;

const unsigned long logInterval = 10000;    // 10 seconds
const unsigned long printInterval = 10000;  // 10 seconds
const unsigned long windowDuration = 30000; // 30 seconds

// Stats variables
float tempMin = 1000;
float tempMax = -1000;
float tempSum = 0;
int tempCount = 0;

uint8_t startHour, startMinute, startSecond;
uint8_t endHour, endMinute, endSecond;

Record rec; // reusable global Record

void loop() {
  // Serial input check
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      lireHistorique();
    }
  }

  unsigned long now = millis();

  // --- Log temperature to EEPROM every 10 seconds ---
  if (now - lastLogMillis >= logInterval) {
    lastLogMillis = now;
    enregistrerTemperature();
  }

  // --- Print live temperature every 10 seconds ---
  if (now - lastPrintMillis >= printInterval) {
    lastPrintMillis = now;
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
      Serial.print("Live Temp: ");
      Serial.print(temp);
      Serial.println(" C");
    } else {
      Serial.println("Live Temp read failed");
    }
  }

  // --- 30 seconds window stats ---
  if (windowStartMillis == 0) {
    // start window
    windowStartMillis = now;
    tempMin = 1000;
    tempMax = -1000;
    tempSum = 0;
    tempCount = 0;
    readTime(rec.year, rec.month, rec.day, startHour, startMinute);
    startSecond = readSeconds();

    lastStatsMeasureMillis = now - logInterval; // so measurement happens immediately
  }

  // Collect stats every 10s inside window
  if ((now - lastStatsMeasureMillis >= logInterval) && (now - windowStartMillis < windowDuration)) {
    lastStatsMeasureMillis = now;
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
      if (temp < tempMin) tempMin = temp;
      if (temp > tempMax) tempMax = temp;
      tempSum += temp;
      tempCount++;
    }
  }

  // Print stats at end of 30s window
  if (windowStartMillis != 0 && now - windowStartMillis >= windowDuration) {
    readTime(rec.year, rec.month, rec.day, endHour, endMinute);
    endSecond = readSeconds();

    Serial.print("Interval ");
    Serial.print(startHour); Serial.print(":"); Serial.print(startMinute); Serial.print(":"); Serial.print(startSecond);
    Serial.print(" - ");
    Serial.print(endHour); Serial.print(":"); Serial.print(endMinute); Serial.print(":"); Serial.print(endSecond);
    Serial.print(" | Min = "); Serial.print(tempMin);
    Serial.print(" C, Max = "); Serial.print(tempMax);
    Serial.print(" C, Moyenne = ");
    if (tempCount > 0)
      Serial.print(tempSum / tempCount);
    else
      Serial.print("N/A");
    Serial.println(" C");

    windowStartMillis = 0; // reset for next window
  }
}

void enregistrerTemperature() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("Erreur lecture temperature !");
    return;
  }

  Record rec;
  readTime(rec.year, rec.month, rec.day, rec.hour, rec.minute);
  rec.temperature = (int8_t)temp;

  int index = trouverDernierIndex() + 1;
  if (index >= MAX_RECORDS) {
    Serial.println("EEPROM pleine, effacement...");
    effacerEEPROM();
    index = 0;
  }

  int addr = EEPROM_START + index * RECORD_SIZE;
  EEPROM.put(addr, rec);

  Serial.print("Mesure enregistrée: ");
  Serial.print(rec.day); Serial.print("/");
  Serial.print(rec.month); Serial.print("/");
  Serial.print(2000 + rec.year); Serial.print(" ");
  Serial.print(rec.hour); Serial.print(":");
  Serial.print(rec.minute); Serial.print(" -> ");
  Serial.print(rec.temperature);
  Serial.println(" C");
}

int trouverDernierIndex() {
  Record rec;
  for (int i = MAX_RECORDS - 1; i >= 0; i--) {
    EEPROM.get(EEPROM_START + i * RECORD_SIZE, rec);
    if (rec.year != 0xFF && rec.year != 0) {
      return i;
    }
  }
  return -1;
}

void effacerEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);
  }
}

void lireHistorique() {
  Serial.println("Historique temperature:");
  Record rec;
  for (int i = 0; i < MAX_RECORDS; i++) {
    EEPROM.get(EEPROM_START + i * RECORD_SIZE, rec);
    if (rec.year == 0xFF || rec.year == 0) break;
    Serial.print(rec.day); Serial.print("/");
    Serial.print(rec.month); Serial.print("/");
    Serial.print(2000 + rec.year); Serial.print(" ");
    Serial.print(rec.hour); Serial.print(":");
    Serial.print(rec.minute); Serial.print(" -> ");
    Serial.print(rec.temperature);
    Serial.println(" C");
  }
  Serial.println("Fin historique.");
}  what about this one?
