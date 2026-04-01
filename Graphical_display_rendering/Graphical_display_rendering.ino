#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <SdFat.h>
// #include <SD.h>
#include <Keypad.h>


unsigned long startTime;
unsigned long elapsedTime;

// 8x8 Micro SD Icon
#define sd_hollow_width 8
#define sd_hollow_height 7
static const unsigned char sd_hollow_bits[] PROGMEM = {
  0x1e, 0x22, 0x42, 0x5a, 0x42, 0x42, 0x7e 
};

static const unsigned char sd_alert[] PROGMEM = {
  0x20, 0x20, 0x20, 0x20, 0x00, 0x20, 0x00, // The '!' Glyph (Top to Bottom)
};


//-------GLCD Setup------
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, 21, 20, 10, 7);  // E (clock), R/W (data), RS (Chip select), RST (reset)
const unsigned char check_LED = 4;


//---RTC Initialization---
RTC_DS3231 rtc;
char monthsOftheYear[12][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};


//---SD Card Initialization---
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CS 14
#define SD_SCK 12
SdFat sd;
File32 dataFile; //Creating of File object for the SD card.
// We use 4MHz for stability on breadboards (4000000)
#define SPI_CONFIG SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(4))
const char* filename = "SDATA.CSV";

//---Keypad Initialization---
const unsigned char rows = 4;
const unsigned char columns = 4;
char keys[rows][columns] = { // All ASCII characters
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

unsigned char row_pins[rows] = {1, 2, 16, 5};
unsigned char column_pins[columns] = {6, 9, 17, 18};
Keypad keypad = Keypad(makeKeymap(keys), row_pins, column_pins, rows, columns);


//---Checking modules---
void module_check() {

  //---Checking RTC module---
  if (!rtc.begin()) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 16, "Couldn't find RTC");
    u8g2.sendBuffer();
    for (unsigned char i = 0; i < 3; i++) {
      digitalWrite(check_LED, HIGH);
      delay(500);
      digitalWrite(check_LED, LOW);
      delay(500);
    }
    delay(1000);
  } 
  // else if (rtc.lostPower()) {
  //   u8g2.clearBuffer();
  //   u8g2.drawStr(0, 16, "RTC lost power");
  //   u8g2.sendBuffer();
  //   digitalWrite(check_LED, HIGH);
  //   delay(1000);
  //   digitalWrite(check_LED, LOW);
  //   delay(1000);
  // }
  else {
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    u8g2.clearBuffer();
    u8g2.drawStr(0, 16, "RTC Init...");
    u8g2.sendBuffer();
    delay(1000);
  }

  //---Checking SD card module---
  if (!sd.begin(SPI_CONFIG)) {
    u8g2.drawStr(0, 25, "Couldn't find SD");
    u8g2.drawStr(0, 34, "module...");
    u8g2.sendBuffer();
    delay(1000);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
  } 
  else {
    u8g2.drawStr(0, 25, "SD module Init...");
    u8g2.sendBuffer();
    delay(1000);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
  }
}

//-------Periodic Data Logging--------
void PDLARS() {
  DateTime now = rtc.now();
  static char date[11];
  sprintf(date, "%02d-%s", now.day(), monthsOftheYear[now.month() - 1]);
  u8g2.drawStr(0, 7, date);
  if (!sd.begin(SPI_CONFIG)) {
    u8g2.drawXBM(114, 0, sd_hollow_width, sd_hollow_height, sd_alert);
    u8g2.drawXBM(120, 0, sd_hollow_width, sd_hollow_height, sd_hollow_bits);  
  } else {
    u8g2.drawXBM(120, 0, sd_hollow_width, sd_hollow_height, sd_hollow_bits);
  }
  u8g2.sendBuffer();

  char dateFull[12];
  sprintf(dateFull, "%02d/%02d/%04d", now.day(), now.month(), now.year());

  char timeFull[15];
  sprintf(timeFull, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  

  String dataString = String(dateFull) + "," + String(timeFull) + "," + " " + "," + " ";

  if (dataFile.open(filename, O_RDWR | O_AT_END)) {
    dataFile.println(dataString);
    delay(100);
    Serial.println("---------------------------------");
    Serial.println("Data succefully written");
    dataFile.sync();
    dataFile.close();
    Serial.println("File succefully closed");
  } else {
    Serial.println("--------------------------");
    Serial.println("Data was not written");
  }
  Serial.println("----------------------------");
  Serial.println(timeFull);

  startTime = millis();
}


void setup() {
  startTime = millis();
  u8g2.begin();
  u8g2.setFont(u8g2_font_profont11_tr);
  
  Wire.begin(19, 38);
  Serial.begin(9600);

  pinMode(check_LED, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  Serial.println("Initializing SD card via SdFat...");
  

  //---FIRST PAGE DISPLAY---
  u8g2.clearBuffer();
  u8g2.drawStr(0, 7, "GREENHOUSE MONITORING");
  u8g2.drawStr(0, 18, " AND DECISION SUPPORT ");
  u8g2.drawStr(0, 29, "        SYSTEM        ");
  u8g2.drawStr(0, 40, "       BY: ASTRO      ");
  u8g2.drawStr(0, 51, "            &         ");
  u8g2.drawStr(0, 62, "           AYO        ");
  u8g2.sendBuffer();
  delay(5000);
  
  module_check();

  if (dataFile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
    Serial.println("---------------------------------");
    Serial.println("SUCCESS: File is open and ready.");
    
    // Optional: Write a header if the file is empty
    if (dataFile.fileSize() == 0) {
      int bytesWritten = dataFile.println("Date,Time,Temp,Humidity");

      Serial.print("Bytes written to header: ");
      Serial.println(bytesWritten);
    }
    
    dataFile.sync(); 
    dataFile.close(); // Always close after setup
    Serial.println("SUCCESS: File synced and safely closed.");
  } else {
    Serial.println("---------------------------------");
    Serial.println("FAILURE: Could not open file.");
    sd.errorPrint(&Serial); // Prints WHY the file open failed
  }

  u8g2.drawFrame(0, 8, 128, 64);
  PDLARS();
}



void loop() {

  char key_press = keypad.getKey();
  if (key_press) {
    Serial.print("---------------");
    Serial.print(key_press);
    Serial.print("---------------");
    Serial.println();
  }

  // u8g2.setFont(u8g2_font_profont11_tr);
  // u8g2.drawFrame(0, 8, 128, 64);
  
  // DateTime now = rtc.now();

  elapsedTime = millis() - startTime;
  if (elapsedTime >= 60000) {   
    PDLARS();    
  }
}

  
  
  
  
  
  
  
  
  
  
  
  
  
  // u8g2.setFont(u8g2_font_profont11_tr);
  // u8g2.clearBuffer();
  // u8g2.drawStr(0,7, "Helloygj,");
  // u8g2.drawStr(0,18, "It's ASTROJOE!");
  // u8g2.sendBuffer();
  // delay(5000);
  // u8g2.clearBuffer();
  // u8g2.drawStr(0, 7, "Cleared page!!!");
  // u8g2.sendBuffer();
  // delay(5000);

