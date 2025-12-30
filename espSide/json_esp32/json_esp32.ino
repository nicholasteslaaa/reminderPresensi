#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include "LittleFS.h"

// LCD Settings
#define SDA_PIN 8
#define SCL_PIN 9
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Button and Logic
#define btn 2
int counter = 0;
bool lastBtnState = LOW;
const char* filename = "/data.json";


// --- SAVE FUNCTION ---
void addData(String key, String value) {
  JsonDocument doc;

  // 1. Try to read existing data first
  if (LittleFS.exists(filename)) {
    File file = LittleFS.open(filename, FILE_READ);
    if (file) {
      deserializeJson(doc, file);
      file.close();
    }
  }

  // 2. Add/Update the new key
  doc[key] = value;

  // 3. Save everything back to the file
  File file = LittleFS.open(filename, FILE_WRITE);
  if (!file) return;
  serializeJson(doc, file);
  file.close();
  
  Serial.println("Updated " + key + " in Flash!");
}

// --- LOAD FUNCTION ---
String getData(String key) {
  if (!LittleFS.exists(filename)) return ""; // Return empty string if no file

  File file = LittleFS.open(filename, FILE_READ);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close(); // Close the file as soon as you are done reading

  if (!error && doc.containsKey(key)) {
    return doc[key].as<String>(); // Safely convert to String
  }
  
  return ""; // Return empty string if key not found
}

void deleteSaveFile() {
  if (LittleFS.exists("/data.json")) {
    if (LittleFS.remove("/data.json")) {
      Serial.println("File deleted successfully");
    } else {
      Serial.println("Delete failed");
    }
  } else {
    Serial.println("File does not exist");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  pinMode(btn, INPUT_PULLDOWN);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    lcd.print("FS Mount Fail");
    return;
  }

  counter = getData("counter").toInt();

}

void loop() {
  bool currentBtnState = digitalRead(btn);

  if (currentBtnState == HIGH && lastBtnState == LOW) {
    delay(50); // Debounce
    counter++;


    lcd.setCursor(0,0);
    lcd.print(String(counter));
    addData("counter",String(counter));
  }

  lastBtnState = currentBtnState;
}