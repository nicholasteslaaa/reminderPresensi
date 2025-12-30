#include <Wire.h>
#include <math.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include "LittleFS.h"


const String pages[] = { "selectingWifi", "typingPassword", "mainMenu", "botPresensi", "setNIM", "setPassword" };
int numOfPages = sizeof(pages) / sizeof(pages[0]);
String currentPage = pages[0];

// --- Global Variables for Error Catching ---
int lastDisconnectReason = 0;

// API
const char* activateBotAPI = "http://192.168.1.6:8000/addBot";
const char* stopBotAPI = "http://192.168.1.6:8000/stopBot";

// save file setting
const char* filename = "/data.json";
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
  if (!LittleFS.exists(filename)) return "";  // Return empty string if no file

  File file = LittleFS.open(filename, FILE_READ);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();  // Close the file as soon as you are done reading

  if (!error && doc.containsKey(key)) {
    return doc[key].as<String>();  // Safely convert to String
  }

  return "";  // Return empty string if key not found
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
////////////////////

// screen setting
#define SDA_PIN 8
#define SCL_PIN 9
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastTimeScreenRefresh = 0;
const long screenRefreshInterval = 250;

char firstRowBuffer[17] = "                ";  // 16 chars + null terminator
char secondRowBuffer[17] = "                ";

void printToScreen(String data, int x, int y) {
  if (y == 0) {
    for (int i = 0; i < 16; i++) {
      if (i < x || (i - x) >= data.length()) {
        firstRowBuffer[i] = ' ';
      } else {
        firstRowBuffer[i] = data[i - x];
      }
    }
    firstRowBuffer[16] = '\0';
  }
  if (y == 1) {
    for (int i = 0; i < 16; i++) {
      if (i < x || (i - x) >= data.length()) {
        secondRowBuffer[i] = ' ';
      } else {
        secondRowBuffer[i] = data[i - x];
      }
    }
    secondRowBuffer[16] = '\0';
  }
}

// vibrationMotor setting
#define vibrationMotor 10

void vibrate(bool ON) {
  pinMode(vibrationMotor, OUTPUT);
  if (ON == true) {
    digitalWrite(vibrationMotor, HIGH);
  } else {
    digitalWrite(vibrationMotor, LOW);
  }
}

bool isPresensiAvailable(String msg) {
  for (int i = 0; i < msg.length(); i++) {
    if (msg[i] == '!') {
      return true;
    }
  }
  return false;
}

////////////////

// button setting
#define upButton 2
#define downButton 3
#define leftButton 4
#define rightButton 5
#define selectButton 6

unsigned long lastTimeButton = 0;
const long buttonInterval = 250;

int leftRightCounter = 0;
int upDownCounter = 0;
bool selectTrigger = false;

char* buttonHandler() {
  int upState = digitalRead(upButton);
  int downState = digitalRead(downButton);
  int rightState = digitalRead(rightButton);
  int leftState = digitalRead(leftButton);
  int selectState = digitalRead(selectButton);

  unsigned long currentTime = millis();
  if (currentTime - lastTimeButton >= buttonInterval) {
    if (upState == 1 || downState == 1 || leftState == 1 || rightState == 1 || selectState == 1) {
      lastTimeButton = currentTime;
      if (upState == 1) return "up";
      if (downState == 1) return "down";
      if (leftState == 1) return "left";
      if (rightState == 1) return "right";
      if (selectState == 1) return "select";
    }
  }
  return "-";
}

// wifi setting
String wifiPassword = "                ";
String selectedSSID = "";
String availableWifi[10];
int availableWifiSize = 10;
bool isWifiConnected = false;

unsigned long lastTimeWifiScan = 0;
const long wifiScanInterval = 10000;

void clearWifiList() {
  for (int i = 0; i < availableWifiSize; i++) {
    availableWifi[i] = "";
  }
}

int getNumOfAvailableWifi() {
  int result = 0;
  for (int i = 0; i < availableWifiSize; i++) {
    if (availableWifi[i] != "") result += 1;
  }
  return result;
}

bool connectingWifi(String ssid, String password) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting To:");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  lastDisconnectReason = 0;
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 10000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);
    if (WiFi.status() == WL_CONNECT_FAILED) break;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
    return true;
  } else {
    if (lastDisconnectReason == 15 || lastDisconnectReason == 2) {
      lcd.setCursor(0, 0);
      lcd.print("Wrong Password!");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Failed/Timeout");
      lcd.setCursor(0, 1);
      lcd.print("Code: " + String(lastDisconnectReason));
    }
    delay(3000);
    return false;
  }
}

void scanningWifi() {
  clearWifiList();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (i < availableWifiSize) {
      availableWifi[i] = WiFi.SSID(i);
    }
  }
}
//////////////////////////////////////////////////////////

// UI setting
bool trigger = false;

String getKeyFromInt(int idx) {
  if (idx == 0) return " ";
  if (idx > 0 && idx <= 10) return String(idx - 1);
  if (idx > 10 && idx <= 36) return String((char)(idx + 54));
  if (idx > 36 && idx <= 63) return String((char)(idx + 59));
  return "";
}

int getIdxFromKey(String data) {
  for (int i = 0; i < 64; i++) {
    if (getKeyFromInt(i) == data) return i;
  }
  return 0;
}

int findPageIdx(String data) {
  for (int i = 0; i < numOfPages; i++) {
    if (data == pages[i]) {
      return i;
    }
  }

  return -1;
}

////////////////////////////////////////////////////////////////

// #### MQTT setting ####
WiFiClient espClient;
PubSubClient client(espClient);
const char* broker = "broker.emqx.io";
bool mqttTrigger = false;
String mqttmessage = "";

void callback(char* topic, byte* payload, unsigned int length) {
  mqttmessage = "";  // Clear old message
  for (int i = 0; i < length; i++) {
    mqttmessage += (char)payload[i];
  }
}

void reconnect() {
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    if (client.connect("ESP32_Client_71231021")) {
      client.subscribe("botPresensi/71231021");
    }
  }
}
/////////////////////////////////////////////////////////////////



// page controller

void changePage(int idx) {
  lcd.clear();
  currentPage = pages[idx];
  trigger = false;
  leftRightCounter = 0;
  upDownCounter = 0;
  wifiPassword = "                ";
  printToScreen("                ", 0, 0);
  printToScreen("                ", 0, 1);
  vibrate(false);
}

void typingPassword() {
  // if has logged check the saved password
  if (getData(selectedSSID) != "") {
    String savedPass = getData(selectedSSID);
    savedPass.trim();
    bool success = connectingWifi(selectedSSID, savedPass);
    if (success == true) {
      changePage(2);
    } else {
      addData(selectedSSID, "");
      ESP.restart();
    }
  }

  char* buttonState = buttonHandler();

  if (trigger == false) {
    trigger = true;
    printToScreen("v", 0, 0);
  }

  if (buttonState == "left") {
    if (leftRightCounter > 0) {
      printToScreen(" ", leftRightCounter, 0);
      leftRightCounter -= 1;
      upDownCounter = getIdxFromKey(String(wifiPassword[leftRightCounter]));
      printToScreen("v", leftRightCounter, 0);
    } else {
      changePage(0);
    }
  }
  if (buttonState == "right" && leftRightCounter < 15) {
    printToScreen(" ", leftRightCounter, 0);
    leftRightCounter += 1;
    upDownCounter = getIdxFromKey(String(wifiPassword[leftRightCounter]));
    printToScreen("v", leftRightCounter, 0);
  }
  if (buttonState == "up") {
    if (upDownCounter < 63) {
      upDownCounter += 1;
    } else {
      upDownCounter = 0;
    }
  }
  if (buttonState == "down") {
    if (upDownCounter > 0) {
      upDownCounter -= 1;
    } else {
      upDownCounter = 63;
    }
  }

  wifiPassword[leftRightCounter] = getKeyFromInt(upDownCounter)[0];
  printToScreen(wifiPassword, 0, 1);

  if (buttonState == "select") {
    String pass = wifiPassword;
    pass.trim();
    bool success = connectingWifi(selectedSSID, pass);
    isWifiConnected = WiFi.status() == WL_CONNECTED;
    if (success == true) {
      addData(selectedSSID, pass);
      changePage(2);
    } else {
      ESP.restart();
    }
  }
}

void selectingWifi() {
  char* buttonState = buttonHandler();
  int totalWifi = getNumOfAvailableWifi();

  if (buttonState == "down" && upDownCounter < totalWifi - 1) upDownCounter += 1;
  if (buttonState == "up" && upDownCounter > 0) upDownCounter -= 1;

  int menuIdx = (upDownCounter / 2) * 2;
  String row1 = (upDownCounter == menuIdx) ? ">" : " ";
  row1 += availableWifi[menuIdx];

  String row2 = " ";
  if (menuIdx + 1 < totalWifi) {
    row2 = (upDownCounter == menuIdx + 1) ? ">" : " ";
    row2 += availableWifi[menuIdx + 1];
  }

  selectedSSID = availableWifi[upDownCounter];
  printToScreen(row1, 0, 0);
  printToScreen(row2, 0, 1);

  if (buttonState == "select") {
    changePage(1);
  }
  if (buttonState == "left") {
    changePage(2);
  }
}

void botPresensi() {
  char* buttonState = buttonHandler();

  if (mqttTrigger == false) {
    mqttTrigger = true;
    client.setServer(broker, 1883);
    client.setCallback(callback);
  }
  reconnect();
  client.loop();

  printToScreen(mqttmessage, 0, 0);
  bool presensiState = isPresensiAvailable(mqttmessage);
  printToScreen("presensi: " + String(presensiState), 0, 1);
  vibrate(presensiState);

  // if (buttonState == "right") {
  //   vibrate(false);
  //   stopBot("http://192.168.1.6:8000/stopBot", "71231021");
  //   changePage(0);
  // }
  if (buttonState == "left") {
    vibrate(false);
    changePage(2);
  }
}

void mainMenu() {
  String optionList[] = { "setNIM", "setPassword", "selectingWifi", "activateBot", "stopBot", "botPresensi" };
  int totalOption = sizeof(optionList) / sizeof(optionList[0]);

  char* buttonState = buttonHandler();

  if (buttonState == "down" && upDownCounter < totalOption - 1) upDownCounter += 1;
  if (buttonState == "up" && upDownCounter > 0) upDownCounter -= 1;

  int menuIdx = (upDownCounter / 2) * 2;
  String row1 = (upDownCounter == menuIdx) ? ">" : " ";
  row1 += optionList[menuIdx];

  String row2 = " ";
  if (menuIdx + 1 < totalOption) {
    row2 = (upDownCounter == menuIdx + 1) ? ">" : " ";
    row2 += optionList[menuIdx + 1];
  }

  // selectedSSID = optionList[upDownCounter];
  printToScreen(row1, 0, 0);
  printToScreen(row2, 0, 1);

  if (buttonState == "select") {
    String selectedOption = optionList[upDownCounter];
    if (selectedOption == optionList[3]){
      if (getData("NIM") != "" && getData("password") != ""){
        activateBot(activateBotAPI,getData("NIM"),getData("password"));
      }else{
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("NIM OR PW");
        lcd.setCursor(0,1);
        lcd.print("Hasn't been set");
        delay(1000);
      }
      changePage(2);
    }else if (selectedOption == optionList[4]){
      if (getData("NIM") != ""){
        stopBot(stopBotAPI,getData("NIM"));
      }else{
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("NIM");
        lcd.setCursor(0,1);
        lcd.print("Hasn't been set");
        delay(1000);
      }
      changePage(2);
    }
    else{
      changePage(findPageIdx(selectedOption));
    }
  }
}

bool noConnectionWarning() {
  if (isWifiConnected == false) {
    lcd.setCursor(0, 0);
    lcd.print("no connection!");
    delay(2000);
    changePage(0);
    return true;
  }
  return false;
}

String NIM = "                ";
void setNIM() {
  if (trigger == false) {
    trigger = true;
    printToScreen("v", 0, 0);
    
    if (getData("NIM") != "") {
      for (int i = 0; i < NIM.length(); i++) {
        NIM[i] = getData("NIM")[i];
        upDownCounter = getIdxFromKey(String(NIM[leftRightCounter]));
      }
    }
  }


  char* buttonState = buttonHandler();

  if (trigger == false) {
    trigger = true;
    printToScreen("v", 0, 0);
  }

  if (buttonState == "left") {
    if (leftRightCounter > 0) {
      printToScreen(" ", leftRightCounter, 0);
      leftRightCounter -= 1;
      upDownCounter = getIdxFromKey(String(NIM[leftRightCounter]));
      printToScreen("v", leftRightCounter, 0);
    } else {
      changePage(2);
    }
  }
  if (buttonState == "right" && leftRightCounter < 15) {
    printToScreen(" ", leftRightCounter, 0);
    leftRightCounter += 1;
    upDownCounter = getIdxFromKey(String(NIM[leftRightCounter]));
    printToScreen("v", leftRightCounter, 0);
  }
  if (buttonState == "up") {
    if (upDownCounter < 63) {
      upDownCounter += 1;
    } else {
      upDownCounter = 0;
    }
  }
  if (buttonState == "down") {
    if (upDownCounter > 0) {
      upDownCounter -= 1;
    } else {
      upDownCounter = 63;
    }
  }

  NIM[leftRightCounter] = getKeyFromInt(upDownCounter)[0];
  printToScreen(NIM, 0, 1);

  if (buttonState == "select") {
    String finalNIM = NIM;
    finalNIM.trim();
    addData("NIM", finalNIM);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NIM has been set");

    delay(1000);

    changePage(2);
  }
}

String password = "                ";
void setPassword() {
  if (trigger == false) {
    trigger = true;
    printToScreen("v", 0, 0);
  }

  if (getData("password") != "") {
    for (int i = 0; i < password.length(); i++) {
      password[i] = getData("password")[i];
      upDownCounter = getIdxFromKey(String(password[leftRightCounter]));
    }
  }

  char* buttonState = buttonHandler();

  if (trigger == false) {
    trigger = true;
    printToScreen("v", 0, 0);
  }

  if (buttonState == "left") {
    if (leftRightCounter > 0) {
      printToScreen(" ", leftRightCounter, 0);
      leftRightCounter -= 1;
      upDownCounter = getIdxFromKey(String(password[leftRightCounter]));
      printToScreen("v", leftRightCounter, 0);
    } else {
      changePage(2);
    }
  }
  if (buttonState == "right" && leftRightCounter < 15) {
    printToScreen(" ", leftRightCounter, 0);
    leftRightCounter += 1;
    upDownCounter = getIdxFromKey(String(password[leftRightCounter]));
    printToScreen("v", leftRightCounter, 0);
  }
  if (buttonState == "up") {
    if (upDownCounter < 63) {
      upDownCounter += 1;
    } else {
      upDownCounter = 0;
    }
  }
  if (buttonState == "down") {
    if (upDownCounter > 0) {
      upDownCounter -= 1;
    } else {
      upDownCounter = 63;
    }
  }

  password[leftRightCounter] = getKeyFromInt(upDownCounter)[0];
  printToScreen(password, 0, 1);

  if (buttonState == "select") {
    String finalPassword = password;
    finalPassword.trim();
    addData("password", finalPassword);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("password set");

    delay(1000);

    changePage(2);
  }
}
//////////////////////////////////////////////////////

//  Http Setting
void stopBot(String serverName, String botId) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // 1. Initialize connection
    http.begin(serverName);

    // 2. IMPORTANT: Increase timeout to 10 seconds to avoid Error -11
    http.setTimeout(10000);

    // 3. Set headers for Form Data
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // 4. Prepare the body
    String httpRequestData = "id=" + botId;

    // 5. Send POST request
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wait for server");
    delay(1000);

    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      String response = http.getString();

      // Print to Serial for debugging
      lcd.setCursor(0, 0);
      lcd.print("Code: " + String(httpResponseCode));
      lcd.setCursor(0, 1);
      lcd.print("Response: " + response);
      delay(1000);

      lcd.clear();
      lcd.setCursor(0, 1);
      // Clean up the JSON string for the small screen
      // If response is {"status":"Bot started"}, we just show "Bot started"
      String cleanMsg = response;
      cleanMsg.replace("{\"status\":\"", "");
      cleanMsg.replace("\"}", "");
      lcd.print(cleanMsg.substring(0, 16));
      delay(1000);

    } else {
      // Handle Error -11 or other connection issues
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error Code:");
      lcd.setCursor(0, 1);
      lcd.print(String(httpResponseCode) + " (Timeout?)");
      delay(1000);
    }

    http.end();
  } else {
    lcd.clear();
    lcd.print("No Connection");
    delay(1000);
  }
}

//  Http Setting
void activateBot(String serverName, String botId, String password) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // 1. Initialize connection
    http.begin(serverName);

    // 2. IMPORTANT: Increase timeout to 10 seconds to avoid Error -11
    http.setTimeout(60000);

    // 3. Set headers for Form Data
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // 4. Prepare the body
    String httpRequestData = "id=" + botId + "&password=" + password;

    // 5. Send POST request
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wait for server");
    delay(1000);

    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      String response = http.getString();

      // Print to Serial for debugging
      lcd.setCursor(0, 0);
      lcd.print("Code: " + String(httpResponseCode));
      lcd.setCursor(0, 1);
      lcd.print("Response: " + response);
      delay(1000);

      lcd.clear();
      lcd.setCursor(0, 1);
      // Clean up the JSON string for the small screen
      // If response is {"status":"Bot started"}, we just show "Bot started"
      String cleanMsg = response;
      cleanMsg.replace("{\"status\":\"", "");
      cleanMsg.replace("\"}", "");
      lcd.print(cleanMsg.substring(0, 16));
      delay(1000);

    } else {
      // Handle Error -11 or other connection issues
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error Code:");
      lcd.setCursor(0, 1);
      lcd.print(String(httpResponseCode) + " (Timeout?)");
      delay(1000);
    }

    http.end();
  } else {
    lcd.clear();
    lcd.print("No Connection");
    delay(1000);
  }
}

///////////////////////////////////////////////////////////////


void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // Setup WiFi Event Listener
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
  },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  pinMode(upButton, INPUT_PULLDOWN);
  pinMode(downButton, INPUT_PULLDOWN);
  pinMode(leftButton, INPUT_PULLDOWN);
  pinMode(rightButton, INPUT_PULLDOWN);
  pinMode(selectButton, INPUT_PULLDOWN);

  lcd.print("Scanning Wifi...");
  scanningWifi();
  lcd.clear();

  lcd.setCursor(0, 0);
  if (!LittleFS.begin(true)) {
    lcd.print("LittleFS Mount Failed");
    return;
  }

  // 2. Check if the file exists. If not, create a blank JSON object.
  if (!LittleFS.exists("/data.json")) {
    lcd.print("file not found");
    delay(1000);
    lcd.print("Init new file");
    delay(1000);

    File file = LittleFS.open(filename, FILE_WRITE);
    if (file) {
      file.print("{}");  // This is a valid, empty JSON object
      file.close();
      lcd.print("File initialized");
      delay(1000);
    }
  } else {
    lcd.print("file found!");
    delay(1000);
  }
}

void loop() {
  unsigned long currentTime = millis();
  isWifiConnected = WiFi.status() == WL_CONNECTED;
  // WiFi Rescan Logic
  if (currentPage == pages[0] && (currentTime - lastTimeWifiScan >= wifiScanInterval)) {
    lastTimeWifiScan = currentTime;
    scanningWifi();
  }

  // Screen Refresh Logic
  if (currentTime - lastTimeScreenRefresh >= screenRefreshInterval) {
    lastTimeScreenRefresh = currentTime;
    lcd.setCursor(0, 0);
    lcd.print(firstRowBuffer);
    lcd.setCursor(0, 1);
    lcd.print(secondRowBuffer);
  }

  // Page Logic
  if (currentPage == pages[0]) selectingWifi();
  else if (currentPage == pages[1]) typingPassword();
  else if (currentPage == pages[2]) mainMenu();
  else if (currentPage == pages[3]) {
    if (noConnectionWarning()) {
      changePage(0);
      return;
    } else {
      botPresensi();
    }
  } else if (currentPage == pages[4]) setNIM();
  else if (currentPage == pages[5]) setPassword();
}