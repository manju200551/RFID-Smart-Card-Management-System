/*
   RFID + I2C LCD system without external libraries
   Board target: Arduino UNO / ATmega328P

   Uses:
   - Hardware UART through Serial
   - Hardware SPI through AVR registers
   - Software I2C (bit-banged) for PCF8574 LCD backpack
   - Manual HD44780 4-bit LCD control through PCF8574
   - Manual MFRC522 register access

   Connections:
   MFRC522:
   SDA(SS) -> D10
   SCK     -> D13
   MOSI    -> D11
   MISO    -> D12
   RST     -> D9
   VCC     -> 3.3V
   GND     -> GND

   LED:
   D7

   LCD I2C backpack (PCF8574):
   SDA -> A4
   SCL -> A5
   VCC -> 5V
   GND -> GND

   Assumed LCD backpack bit mapping:
   P0=RS, P1=RW, P2=EN, P3=BL, P4=D4, P5=D5, P6=D6, P7=D7
   Typical address: 0x27
*/

#include <Arduino.h>

// ================= PIN DEFINITIONS =================
#define SS_PIN   10
#define RST_PIN  9
#define LED_PIN  7

#define I2C_SDA  A4
#define I2C_SCL  A5

#define LCD_ADDR 0x27

// ================= LCD BIT MAP =================
#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BL 0x08

// ================= MFRC522 REGISTERS =================
#define CommandReg      0x01
#define ComIEnReg       0x02
#define DivIEnReg       0x03
#define ComIrqReg       0x04
#define DivIrqReg       0x05
#define ErrorReg        0x06
#define Status1Reg      0x07
#define Status2Reg      0x08
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define ControlReg      0x0C
#define BitFramingReg   0x0D
#define CollReg         0x0E
#define ModeReg         0x11
#define TxModeReg       0x12
#define RxModeReg       0x13
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define VersionReg      0x37

// ================= MFRC522 COMMANDS =================
#define PCD_Idle        0x00
#define PCD_Mem         0x01
#define PCD_CalcCRC     0x03
#define PCD_Transmit    0x04
#define PCD_NoCmdChange 0x07
#define PCD_Receive     0x08
#define PCD_Transceive  0x0C
#define PCD_SoftReset   0x0F

// ================= PICC COMMANDS =================
#define PICC_CMD_REQA       0x26
#define PICC_CMD_WUPA       0x52
#define PICC_CMD_SEL_CL1    0x93
#define PICC_CMD_HLTA       0x50

struct CardUser {
  byte uid[4];
  const char* role;
  const char* name;
  const char* college;
  float balance;
};

// ================= CARD DATABASE =================
CardUser users[] = {
  {{0x59, 0x14, 0x63, 0xD5}, "MASTER",  "Perumal",   "Suguna", 1000.00},
  {{0x20, 0xBC, 0xBE, 0x3A}, "SLAVE 1", "Govindhan", "PSG",     850.00},
  {{0x40, 0x41, 0x70, 0x38}, "SLAVE 2", "Kaviya",    "NGP",     920.00},
  {{0x50, 0x01, 0x70, 0x38}, "SLAVE 3", "Manju",     "Park",    760.00}
};

const int totalUsers = sizeof(users) / sizeof(users[0]);

bool masterVerified = false;
bool slave1Done = false;
bool slave2Done = false;
bool slave3Done = false;

// ================= RFID UID BUFFER =================
byte rfidUID[10];
byte rfidUIDSize = 0;

// ================= LOW LEVEL I2C =================
void i2cDelay() {
  delayMicroseconds(5);
}

void sdaHigh() {
  pinMode(I2C_SDA, INPUT_PULLUP);
}

void sdaLow() {
  pinMode(I2C_SDA, OUTPUT);
  digitalWrite(I2C_SDA, LOW);
}

void sclHigh() {
  pinMode(I2C_SCL, INPUT_PULLUP);
}

void sclLow() {
  pinMode(I2C_SCL, OUTPUT);
  digitalWrite(I2C_SCL, LOW);
}

void i2cInit() {
  sdaHigh();
  sclHigh();
}

void i2cStart() {
  sdaHigh();
  sclHigh();
  i2cDelay();
  sdaLow();
  i2cDelay();
  sclLow();
}

void i2cStop() {
  sdaLow();
  i2cDelay();
  sclHigh();
  i2cDelay();
  sdaHigh();
  i2cDelay();
}

bool i2cWriteByte(byte data) {
  for (byte i = 0; i < 8; i++) {
    if (data & 0x80) sdaHigh();
    else sdaLow();

    i2cDelay();
    sclHigh();
    i2cDelay();
    sclLow();
    data <<= 1;
  }

  sdaHigh();
  i2cDelay();
  sclHigh();
  bool ack = (digitalRead(I2C_SDA) == 0);
  i2cDelay();
  sclLow();
  return ack;
}

void pcf8574Write(byte data) {
  i2cStart();
  i2cWriteByte((LCD_ADDR << 1) | 0);
  i2cWriteByte(data);
  i2cStop();
}

// ================= LCD FUNCTIONS =================
byte lcdBacklight = LCD_BL;

void lcdPulseEnable(byte data) {
  pcf8574Write(data | LCD_EN | lcdBacklight);
  delayMicroseconds(1);
  pcf8574Write((data & ~LCD_EN) | lcdBacklight);
  delayMicroseconds(50);
}

void lcdWrite4Bits(byte nibble, byte mode) {
  byte data = 0;
  if (mode) data |= LCD_RS;

  if (nibble & 0x01) data |= 0x10;
  if (nibble & 0x02) data |= 0x20;
  if (nibble & 0x04) data |= 0x40;
  if (nibble & 0x08) data |= 0x80;

  lcdPulseEnable(data);
}

void lcdSend(byte value, byte mode) {
  lcdWrite4Bits((value >> 4) & 0x0F, mode);
  lcdWrite4Bits(value & 0x0F, mode);
}

void lcdCommand(byte cmd) {
  lcdSend(cmd, 0);
  if (cmd == 0x01 || cmd == 0x02) delay(2);
}

void lcdWriteChar(char c) {
  lcdSend((byte)c, 1);
}

void lcdInit() {
  delay(50);
  pcf8574Write(lcdBacklight);
  delay(1000);

  lcdWrite4Bits(0x03, 0);
  delay(5);
  lcdWrite4Bits(0x03, 0);
  delayMicroseconds(150);
  lcdWrite4Bits(0x03, 0);
  lcdWrite4Bits(0x02, 0);

  lcdCommand(0x28); // 4-bit, 2-line, 5x8
  lcdCommand(0x0C); // display on, cursor off
  lcdCommand(0x06); // entry mode
  lcdCommand(0x01); // clear
  delay(2);
}

void lcdClear() {
  lcdCommand(0x01);
  delay(2);
}

void lcdSetCursor(byte col, byte row) {
  byte row_offsets[] = {0x00, 0x40, 0x14, 0x54};
  lcdCommand(0x80 | (col + row_offsets[row]));
}

void lcdPrint(const char *str) {
  while (*str) lcdWriteChar(*str++);
}

void lcdPrintString(String s) {
  for (unsigned int i = 0; i < s.length(); i++) {
    lcdWriteChar(s[i]);
  }
}

void lcdPrintFloat(float value, byte decimals) {
  char buf[20];
  dtostrf(value, 0, decimals, buf);
  lcdPrint(buf);
}

// ================= LOW LEVEL SPI =================
void spiInit() {
  pinMode(SS_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);

  pinMode(11, OUTPUT); // MOSI
  pinMode(12, INPUT);  // MISO
  pinMode(13, OUTPUT); // SCK

  digitalWrite(SS_PIN, HIGH);

  // SPI enable, master, Fosc/16
  SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
  SPSR = 0;
}

byte spiTransferByte(byte data) {
  SPDR = data;
  while (!(SPSR & _BV(SPIF)));
  return SPDR;
}

// ================= MFRC522 LOW LEVEL =================
void rfidWriteReg(byte reg, byte value) {
  digitalWrite(SS_PIN, LOW);
  spiTransferByte((reg << 1) & 0x7E);
  spiTransferByte(value);
  digitalWrite(SS_PIN, HIGH);
}

byte rfidReadReg(byte reg) {
  digitalWrite(SS_PIN, LOW);
  spiTransferByte(((reg << 1) & 0x7E) | 0x80);
  byte value = spiTransferByte(0x00);
  digitalWrite(SS_PIN, HIGH);
  return value;
}

void rfidSetBitMask(byte reg, byte mask) {
  byte tmp = rfidReadReg(reg);
  rfidWriteReg(reg, tmp | mask);
}

void rfidClearBitMask(byte reg, byte mask) {
  byte tmp = rfidReadReg(reg);
  rfidWriteReg(reg, tmp & (~mask));
}

void rfidAntennaOn() {
  byte temp = rfidReadReg(TxControlReg);
  if ((temp & 0x03) != 0x03) {
    rfidSetBitMask(TxControlReg, 0x03);
  }
}

void rfidReset() {
  rfidWriteReg(CommandReg, PCD_SoftReset);
  delay(50);
}

void rfidInit() {
  digitalWrite(RST_PIN, HIGH);
  delay(50);

  rfidReset();

  rfidWriteReg(TModeReg, 0x8D);
  rfidWriteReg(TPrescalerReg, 0x3E);
  rfidWriteReg(TReloadRegL, 30);
  rfidWriteReg(TReloadRegH, 0);
  rfidWriteReg(TxASKReg, 0x40);
  rfidWriteReg(ModeReg, 0x3D);

  rfidAntennaOn();
}

// ================= RFID CORE =================
byte rfidToCard(byte command, byte *sendData, byte sendLen, byte *backData, unsigned int *backLen) {
  byte status = 1;
  byte irqEn = 0x00;
  byte waitIRq = 0x00;
  byte lastBits;
  byte n;
  unsigned int i;

  if (command == PCD_Transceive) {
    irqEn = 0x77;
    waitIRq = 0x30;
  }

  rfidWriteReg(ComIEnReg, irqEn | 0x80);
  rfidClearBitMask(ComIrqReg, 0x80);
  rfidSetBitMask(FIFOLevelReg, 0x80);
  rfidWriteReg(CommandReg, PCD_Idle);

  for (i = 0; i < sendLen; i++) {
    rfidWriteReg(FIFODataReg, sendData[i]);
  }

  rfidWriteReg(CommandReg, command);

  if (command == PCD_Transceive) {
    rfidSetBitMask(BitFramingReg, 0x80);
  }

  i = 2000;
  do {
    n = rfidReadReg(ComIrqReg);
    i--;
  } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

  rfidClearBitMask(BitFramingReg, 0x80);

  if (i != 0) {
    if ((rfidReadReg(ErrorReg) & 0x1B) == 0x00) {
      status = 0;

      if (n & 0x01) status = 2;

      if (command == PCD_Transceive) {
        n = rfidReadReg(FIFOLevelReg);
        lastBits = rfidReadReg(ControlReg) & 0x07;

        if (lastBits) *backLen = (n - 1) * 8 + lastBits;
        else *backLen = n * 8;

        if (n == 0) n = 1;
        if (n > 16) n = 16;

        for (i = 0; i < n; i++) {
          backData[i] = rfidReadReg(FIFODataReg);
        }
      }
    } else {
      status = 1;
    }
  }

  return status;
}

bool rfidRequest(byte reqMode, byte *tagType) {
  byte status;
  unsigned int backBits;

  rfidWriteReg(BitFramingReg, 0x07);
  tagType[0] = reqMode;

  status = rfidToCard(PCD_Transceive, tagType, 1, tagType, &backBits);

  return (status == 0 && backBits == 0x10);
}

bool rfidAnticoll(byte *serNum) {
  byte status;
  byte i;
  byte serNumCheck = 0;
  unsigned int unLen;

  rfidWriteReg(BitFramingReg, 0x00);

  serNum[0] = PICC_CMD_SEL_CL1;
  serNum[1] = 0x20;

  status = rfidToCard(PCD_Transceive, serNum, 2, serNum, &unLen);

  if (status == 0) {
    for (i = 0; i < 4; i++) serNumCheck ^= serNum[i];
    if (serNumCheck != serNum[4]) status = 1;
  }

  return (status == 0);
}

void rfidHalt() {
  byte buff[4];
  unsigned int unLen;

  buff[0] = PICC_CMD_HLTA;
  buff[1] = 0x00;
  rfidToCard(PCD_Transceive, buff, 2, buff, &unLen);
}

bool rfidIsNewCardPresent() {
  byte tagType[2];
  return rfidRequest(PICC_CMD_REQA, tagType);
}

bool rfidReadCardSerial() {
  byte serNum[5];
  if (rfidAnticoll(serNum)) {
    rfidUIDSize = 4;
    for (byte i = 0; i < 4; i++) {
      rfidUID[i] = serNum[i];
    }
    return true;
  }
  return false;
}

// ================= UTILITIES =================
String uidToString(byte *buffer, byte bufferSize) {
  String uid = "";
  for (byte i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) uid += "0";
    uid += String(buffer[i], HEX);
    if (i < bufferSize - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

int findUserIndex(byte *uid, byte uidSize) {
  if (uidSize != 4) return -1;

  for (int i = 0; i < totalUsers; i++) {
    bool match = true;
    for (byte j = 0; j < 4; j++) {
      if (users[i].uid[j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return i;
  }

  return -1;
}

void resetSystem() {
  masterVerified = false;
  slave1Done = false;
  slave2Done = false;
  slave3Done = false;
}

void welcomeScreen() {
  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint("SCAN MASTER");
  lcdSetCursor(0, 1);
  lcdPrint("CARD");
}

void showUserInfo(int index) {
  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint(users[index].role);
  lcdSetCursor(0, 1);
  lcdPrint(users[index].name);
  delay(2000);

  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint("College:");
  lcdSetCursor(0, 1);
  lcdPrint(users[index].college);
  delay(2000);

  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint("Balance:");
  lcdSetCursor(0, 1);
  lcdPrint("Rs.");
  lcdPrintFloat(users[index].balance, 2);
  delay(2500);
}

void updateBalance(int index) {
  Serial.println("--------------------------------");
  Serial.println("Type Command If Needed");
  Serial.println("C1000 = Credit 1000");
  Serial.println("D1000  = Debit 1000");
  Serial.println("Press ENTER to Skip");
  Serial.println("--------------------------------");

  while (!Serial.available()) {
    // wait for input
  }

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.length() == 0) {
    Serial.println("Transaction Skipped");
    return;
  }

  char type = cmd.charAt(0);
  float amt = cmd.substring(1).toFloat();

  Serial.print("Old Balance : Rs.");
  Serial.println(users[index].balance, 2);

  if (type == 'C' || type == 'c') {
    users[index].balance += amt;
    Serial.print("Credited : Rs.");
    Serial.println(amt, 2);
  } else if (type == 'D' || type == 'd') {
    users[index].balance -= amt;
    Serial.print("Debited : Rs.");
    Serial.println(amt, 2);
  } else {
    Serial.println("Invalid Command");
    return;
  }

  Serial.print("Updated Balance : Rs.");
  Serial.println(users[index].balance, 2);
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  i2cInit();
  lcdInit();

  spiInit();
  rfidInit();

  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint("RFID SYSTEM");
  lcdSetCursor(0, 1);
  lcdPrint("READY");
  delay(2000);

  welcomeScreen();

  Serial.println("RFID MULTI CARD SYSTEM READY");
  Serial.print("MFRC522 Version: 0x");
  Serial.println(rfidReadReg(VersionReg), HEX);
}

// ================= LOOP =================
void loop() {
  if (!rfidIsNewCardPresent()) return;
  if (!rfidReadCardSerial()) return;

  String scannedUID = uidToString(rfidUID, rfidUIDSize);

  Serial.print("Scanned UID: ");
  Serial.println(scannedUID);

  int userIndex = findUserIndex(rfidUID, rfidUIDSize);

  // ================= UNKNOWN CARD =================
  if (userIndex < 0) {
    Serial.println("UNKNOWN CARD");

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("UNKNOWN CARD");
    lcdSetCursor(0, 1);
    lcdPrint("ACCESS DENIED");

    delay(2000);
    welcomeScreen();
    rfidHalt();
    return;
  }

  // ================= MASTER CARD =================
  if (userIndex == 0) {
    resetSystem();
    masterVerified = true;

    Serial.println("MASTER VERIFIED");
    Serial.print("Username : ");
    Serial.println(users[userIndex].name);
    Serial.print("College  : ");
    Serial.println(users[userIndex].college);
    Serial.print("Balance  : Rs.");
    Serial.println(users[userIndex].balance, 2);

    digitalWrite(LED_PIN, HIGH);

    showUserInfo(userIndex);   // Master info show aagum

    // updateBalance(userIndex); // Uncomment if master-ku credit/debit venum

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("MASTER OK");
    lcdSetCursor(0, 1);
    lcdPrint("SCAN SLAVES");

    delay(2000);
    digitalWrite(LED_PIN, LOW);

    rfidHalt();
    return;
  }

  // ================= MASTER CHECK =================
  if (!masterVerified) {
    Serial.println("SCAN MASTER FIRST");

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("MASTER FIRST");
    lcdSetCursor(0, 1);
    lcdPrint("ACCESS DENIED");

    delay(2000);
    welcomeScreen();
    rfidHalt();
    return;
  }

  // ================= SLAVE 1 =================
  if (userIndex == 1 && !slave1Done) {
    slave1Done = true;
    digitalWrite(LED_PIN, HIGH);

    Serial.println("SLAVE 1 VERIFIED");
    Serial.print("Username : ");
    Serial.println(users[userIndex].name);
    Serial.print("College  : ");
    Serial.println(users[userIndex].college);
    Serial.print("Balance  : Rs.");
    Serial.println(users[userIndex].balance, 2);

    showUserInfo(userIndex);
    updateBalance(userIndex);

    digitalWrite(LED_PIN, LOW);
  }

  // ================= SLAVE 2 =================
  else if (userIndex == 2 && !slave2Done) {
    slave2Done = true;
    digitalWrite(LED_PIN, HIGH);

    Serial.println("SLAVE 2 VERIFIED");
    Serial.print("Username : ");
    Serial.println(users[userIndex].name);
    Serial.print("College  : ");
    Serial.println(users[userIndex].college);
    Serial.print("Balance  : Rs.");
    Serial.println(users[userIndex].balance, 2);

    showUserInfo(userIndex);
    updateBalance(userIndex);

    digitalWrite(LED_PIN, LOW);
  }

  // ================= SLAVE 3 =================
  else if (userIndex == 3 && !slave3Done) {
    slave3Done = true;
    digitalWrite(LED_PIN, HIGH);

    Serial.println("SLAVE 3 VERIFIED");
    Serial.print("Username : ");
    Serial.println(users[userIndex].name);
    Serial.print("College  : ");
    Serial.println(users[userIndex].college);
    Serial.print("Balance  : Rs.");
    Serial.println(users[userIndex].balance, 2);

    showUserInfo(userIndex);
    updateBalance(userIndex);

    digitalWrite(LED_PIN, LOW);
  }

  // ================= ALREADY SCANNED =================
  else {
    Serial.println("CARD ALREADY SCANNED");

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("ALREADY");
    lcdSetCursor(0, 1);
    lcdPrint("SCANNED");
    delay(2000);
  }

  // ================= ALL DONE =================
  if (slave1Done && slave2Done && slave3Done) {
    Serial.println("ALL 3 SLAVES VERIFIED");
    Serial.println("SESSION COMPLETE");

    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("ALL DONE");
    lcdSetCursor(0, 1);
    lcdPrint("RESETTING");

    delay(3000);
    resetSystem();
    welcomeScreen();
  }

  rfidHalt();
}
C CODE