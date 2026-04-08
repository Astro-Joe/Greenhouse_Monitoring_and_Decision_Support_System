#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <SdFat.h>
// #include <SD.h>
#include <Keypad.h>

unsigned long startTime;
unsigned long elapsedTime;
bool condition_check;
bool sdCardFlag = false;


//======8x8 Micro SD Icon======
#define sd_hollow_width 8
#define sd_hollow_height 7
static const unsigned char sd_hollow_bits[] PROGMEM = {
  0x1e, 0x22, 0x42, 0x5a, 0x42, 0x42, 0x7e 
};

static const unsigned char sd_alert[] PROGMEM = {
  0x20, 0x20, 0x20, 0x20, 0x00, 0x20, 0x00, // The '!' Glyph (Top to Bottom)
};


//======GLCD Setup======
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, 21, 20, 10, 7);  // E (clock), R/W (data), RS (Chip select), RST (reset)
const unsigned char check_LED = 4;


//======RTC Initialization======
RTC_DS3231 rtc;
char monthsOftheYear[12][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};


//======SD Card Initialization======
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CS 14
#define SD_SCK 12
SdFat sd;
File32 dataFile; //Creating of File object for the SD card.
// We use 4MHz for stability on breadboards (4000000)
#define SPI_CONFIG SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(4))
const char* filename = "SDATA.CSV";

//======Keypad Initialization======
const unsigned char rows = 4;
const unsigned char columns = 4;
char keys[rows][columns] = { // All ASCII characters
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'D'},
  {'C', '0', 'E', 'F'}
};

unsigned char row_pins[rows] = {1, 2, 16, 5};
unsigned char column_pins[columns] = {6, 9, 17, 18};
Keypad keypad = Keypad(makeKeymap(keys), row_pins, column_pins, rows, columns);


enum ScreenState {
  DEFAULT_STATE,
  PROJECT_DISPLAY, //Showing the title of the projects and people involved.
  MAIN_MENU, //Showing the options to access other screens.
  ATMOSPHERIC_DATA, // Air temp, Relative humidity, Ambient light, CO2, atmospheric pressure. 
  SOIL_DATA, // Soil temp, Soil moisture, soil pH
  DECISION_SUPPORT, // System recommendations and alerts 
  SYSTEM_SETTINGS, // SD card logging, Wi-Fi/4G connection, resetting the time, manually fixing sensor thresholds etc.
  GRAPHS, // Display simple relationship graphs.
  CODE, // Page to input special codes.
}; 
ScreenState currentState = DEFAULT_STATE;


//=======Loading Animation Screen=======
void loading_animation(unsigned char col, unsigned char row) {
  unsigned char cycle = 0;   
  while (cycle < 4) {   
    switch (cycle) {
      case 0:
        u8g2.drawStr(col, row, "."); 
        u8g2.sendBuffer();
        break;
      case 1:
        u8g2.drawStr(col, row, ".."); 
        u8g2.sendBuffer();
        break;
      case 2:
        u8g2.drawStr(col, row, "..."); 
        u8g2.sendBuffer();
        break;
      case 3:
        u8g2.drawStr(col, row, "   "); 
        u8g2.sendBuffer();
        break;
    }
    delay(300);
    cycle++;
  }
}
        

//======Displaying project title======
void projectDisplay () { 
  u8g2.clearBuffer();
  delay(600);
  u8g2.drawStr(1, 7, "GREENHOUSE MONITORING");
  u8g2.drawStr(0, 18, " AND DECISION SUPPORT ");
  u8g2.drawStr(0, 29, "        SYSTEM        ");
  u8g2.drawStr(0, 40, "BY: ILEMOBAYO JOSEPH  ");
  u8g2.drawStr(0, 51, "            &         ");
  u8g2.drawStr(0, 62, "    OLAYINKA AYOMIDE  ");
  u8g2.sendBuffer();
  delay(5000);
  Serial.println(">> Project display successfully ran!");  
}


//======Checking modules======
void module_check() {

  //---Checking RTC module---
  if (!rtc.begin()) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 16, "Couldn't find RTC");
    u8g2.sendBuffer();
    Serial.println(">> Couldn't find RTC");
    for (unsigned char i = 0; i < 3; i++) {
      digitalWrite(check_LED, HIGH);
      delay(500);
      digitalWrite(check_LED, LOW);
      delay(500);
    }
    delay(300);
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
    u8g2.drawStr(0, 16, "RTC Init");
    u8g2.sendBuffer();
    delay(100);
    loading_animation(49, 16);
    Serial.println(">> RTC Initialized successfully");
    // delay(300);
  }

  //---Checking SD card module---
  if (!sd.begin(SPI_CONFIG)) {
    sdCardFlag = true;
    u8g2.drawStr(0, 25, "Couldn't find SD");
    u8g2.drawStr(0, 34, "module");
    u8g2.sendBuffer();
    Serial.println(">> Couldn't find SD module");
    delay(1000);
    // u8g2.clearBuffer();
    // u8g2.sendBuffer();
  } 
  else {
    u8g2.drawStr(0, 25, "SD module Init");
    u8g2.sendBuffer();
    delay(100);
    loading_animation(84, 25);
    Serial.println(">> SD module Initialized successfully");
    delay(1000);
    // u8g2.clearBuffer();
    // u8g2.sendBuffer();
  }
}


//======Status Bar======
void status_bar() {
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 7, 128, 64);
  u8g2.drawHLine(0, 17, 128);
  u8g2.drawHLine(0, 63, 128);
  // u8g2.drawLine
  DateTime now = rtc.now();
  static char date[11];
  sprintf(date, "%02d-%s", now.day(), monthsOftheYear[now.month() - 1]);
  u8g2.drawStr(0, 7, date);
  if (sdCardFlag) {
    u8g2.drawXBM(114, 0, sd_hollow_width, sd_hollow_height, sd_alert);
    u8g2.drawXBM(120, 0, sd_hollow_width, sd_hollow_height, sd_hollow_bits);  
  } else {
    u8g2.drawXBM(120, 0, sd_hollow_width, sd_hollow_height, sd_hollow_bits);
  }
  // u8g2.sendBuffer();
  // Serial.println(">> Status bar successfully updated.");
}


//======Showing the options to access other screens======
void mainMenu(){
  status_bar();
  u8g2.drawStr(36, 16, "MAIN MENU");
  u8g2.drawStr(2, 26, "1. ATMOSPHERIC DATA");
  u8g2.drawStr(2, 35, "2. SOIL DATA");
  u8g2.drawStr(2, 44, "3. DECISION SUPPORT");
  u8g2.drawStr(2, 53, "4. SYSTEM SETTINGS");
  u8g2.drawStr(2, 62, "5. GRAPHS");
}


//======Atmospheric Data Page======
void atm_data() {
  status_bar();
  u8g2.drawStr(17, 16, "ATMOSPHERIC DATA");
  u8g2.drawStr(2, 26, "AIR TEMP: ");
  u8g2.drawStr(2, 35, "R-HUMIDITY: ");
  u8g2.drawStr(2, 45, "AMBIENT LIGHT: ");
  u8g2.drawStr(2, 54, "CO2 CONC: ");
  u8g2.drawStr(2, 63, "ATM PRESSURE: ");
}


//======Soil Data Page======
void soil_data(){
  status_bar();
  u8g2.drawStr(36, 16, "SOIL DATA");
  u8g2.drawStr(2, 26, "SOIL TEMP: ");
  u8g2.drawStr(2, 35, "SOIL MOISTURE: ");
  u8g2.drawStr(2, 45, "SOIL pH: ");
}


//=====Decision Support Page====
void decision_support() {
  status_bar();
  u8g2.drawStr(17, 16, "DECISION SUPPORT");
  u8g2.drawStr(33, 36, "PAGE UNDER");
  u8g2.drawStr(26, 45, "CONSTRUCTION!");
}


//======System Status Page======
void sys_settings() {
  status_bar();
  u8g2.drawStr(24, 16, "SYSTEM SETTINGS");
  u8g2.drawStr(33, 36, "PAGE UNDER");
  u8g2.drawStr(26, 45, "CONSTRUCTION!");
}


//=======System Settings Page=====
void graphs() {
  status_bar();
  u8g2.drawStr(45, 16, "GRAPHS");
  u8g2.drawStr(33, 36, "PAGE UNDER");
  u8g2.drawStr(26, 45, "CONSTRUCTION!");
} 


//======Page for inputting special codes======
void codeInputPage() {
  status_bar();
  u8g2.drawStr(2, 16, "Enter code: ");
  u8g2.sendBuffer();
}


//======Periodic Data Logging======
void PDL() {
  DateTime now = rtc.now();
  char dateFull[12];
  sprintf(dateFull, "%02d/%02d/%04d", now.day(), now.month(), now.year());

  char timeFull[15];
  sprintf(timeFull, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  

  String dataString = String(dateFull) + "," + String(timeFull) + "," + " " + "," + " ";

  if (dataFile.open(filename, O_RDWR | O_AT_END)) {
    dataFile.println(dataString);
    delay(100);
    Serial.println(">> Data succefully written");
    dataFile.sync();
    dataFile.close();
    Serial.println(">> File succefully closed");
  } else {
    
    Serial.println(">> Data was not written");
  }
  Serial.println("----------------------------");
  Serial.println(timeFull);

  startTime = millis();
}


//======Special code to display the project display======
void special_code() {
  String project_display_code = "1601"; // special code for project title screen
  String userInput = "";
  condition_check = true;
  Serial.print(">> User Input: ");
  Serial.println(userInput);


  Serial.println(">> special_code function running");
  while (condition_check) {
    char key = keypad.getKey();
    if (key) {
      if (key == 'E') {
        if (userInput == project_display_code){
          Serial.println(">> Correct code!");
          condition_check = false;
          currentState = PROJECT_DISPLAY;
        }
        else if (userInput != project_display_code) {
          Serial.println(">> Wrong code");
          u8g2.clearBuffer();
          u8g2.drawStr(2, 17, "WRONG CODE!!!");
          u8g2.sendBuffer();
          delay(1500);
          condition_check = false;
          currentState = CODE;
        }
        continue;
      }   
      
      if (key == 'C') {
        userInput = "";
        Serial.println(">> Code input cleared");
        Serial.print(">> User Input: ");
        Serial.println(userInput);
        continue;
      }  

      //====Standard method to escape special code mode====
      if (userInput == "FF") {
        condition_check = false;
        currentState = MAIN_MENU;   
      }
    
      userInput += key;
      Serial.println(">> Appended input to userInput");
      Serial.print(">> User Input: ");
      Serial.println(userInput);
    } 
  }
}




void setup() {
  u8g2.begin();
  u8g2.setFont(u8g2_font_profont11_tr);
  
  Wire.begin(19, 38);
  Serial.begin(115200);

  pinMode(check_LED, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  Serial.println(">> Initializing SD card via SdFat...");
  
  projectDisplay();
  u8g2.sendBuffer();

  module_check();

  status_bar();
  mainMenu();
  u8g2.sendBuffer();
  
  
  if (dataFile.open(filename, O_RDWR | O_CREAT | O_AT_END)) { 
    Serial.println(">> SUCCESS: File is open and ready.");
    
    // Optional: Write a header if the file is empty
    if (dataFile.fileSize() == 0) {
      int bytesWritten = dataFile.println("Date,Time,Temp,Humidity");

      Serial.print("Bytes written to header: ");
      Serial.println(bytesWritten);
    }
    
    dataFile.sync(); 
    dataFile.close(); // Always close after setup
    Serial.println(">> SUCCESS: File synced and safely closed.");
  } else { 
    Serial.println(">> FAILURE: Could not open file.");
    sd.errorPrint(&Serial); // Prints WHY the file open failed
  }

  PDL();
  startTime = millis();
}


void loop() {
  char key_press = keypad.getKey();
  if (key_press) {
    Serial.print("---------------");
    Serial.print(key_press);
    Serial.print("---------------");
    Serial.println();
  }

  switch (key_press) {
    case 'C':
      currentState = CODE;
      break;
    case '0':
      currentState = MAIN_MENU;
      break;
    case '1':
      currentState = ATMOSPHERIC_DATA;
      break;
    case '2':
      currentState = SOIL_DATA;
      break;
    case '3':
      currentState = DECISION_SUPPORT;
      break;
    case '4':
      currentState = SYSTEM_SETTINGS;
      break;
    case '5': 
      currentState = GRAPHS;
      break;
  }


  switch (currentState) {
    case PROJECT_DISPLAY:
      projectDisplay();
      u8g2.sendBuffer();
      currentState = MAIN_MENU; 
      break;
    case MAIN_MENU:
      mainMenu();
      u8g2.sendBuffer();
      currentState = DEFAULT_STATE;
      break;
    case ATMOSPHERIC_DATA:
      atm_data();
      u8g2.sendBuffer();
      currentState = DEFAULT_STATE;
      break;
    case SOIL_DATA:
      soil_data();
      u8g2.sendBuffer();
      currentState = DEFAULT_STATE;
      break;
    case DECISION_SUPPORT:
      decision_support();
      u8g2.sendBuffer();
      currentState = DEFAULT_STATE;
      break;
    case SYSTEM_SETTINGS:
      sys_settings();
      u8g2.sendBuffer();
      currentState = DEFAULT_STATE;
      break;
    case GRAPHS:
      graphs();
      u8g2.sendBuffer();
      currentState = DEFAULT_STATE;
      break;
    case CODE:
      codeInputPage();
      special_code();
      break;
  }


  elapsedTime = millis() - startTime;
  if (elapsedTime >= 60000) {   
    PDL();    
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

