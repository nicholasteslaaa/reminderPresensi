#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Screen settings
#define SDA_PIN 8
#define SCL_PIN 9
LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "Cahaya Kehidupan";
const char* password = "12101979";

// Your computer's IP and FastAPI port
const char* serverName = "http://192.168.1.6:8000/addBot";

#define upButton 2
bool trigger = false; // Prevents multiple sends on one press

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  // Initialize WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected!");
  lcd.clear();
  lcd.print("Ready...");

  pinMode(upButton, INPUT_PULLDOWN);
}

void loop() {
  int btnState = digitalRead(upButton);

  // Trigger when button is pressed (HIGH) and trigger flag is false
  if (btnState == HIGH && !trigger) {
    trigger = true; 
    lcd.clear();
    lcd.print("Sending Request");
    sendAddBotRequest("71231021", "460066");
  } 
  
  // Reset trigger flag when button is released
  if (btnState == LOW) {
    trigger = false;
  }
}

void sendAddBotRequest(String botId, String botPass) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // 1. Initialize connection
    http.begin(serverName);
    
    // 2. IMPORTANT: Increase timeout to 10 seconds to avoid Error -11
    http.setTimeout(10000); 
    
    // 3. Set headers for Form Data
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // 4. Prepare the body
    String httpRequestData = "id=" + botId + "&password=" + botPass;

    // 5. Send POST request
    Serial.println("Waiting for server...");
    int httpResponseCode = http.POST(httpRequestData);

    lcd.clear();
    if (httpResponseCode > 0) {
      String response = http.getString();
      
      // Print to Serial for debugging
      Serial.println("Code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
      
      // Print to LCD (Top row: Code, Bottom row: Message)
      lcd.setCursor(0, 0);
      lcd.print("HTTP Code: " + String(httpResponseCode));
      
      lcd.setCursor(0, 1);
      // Clean up the JSON string for the small screen
      // If response is {"status":"Bot started"}, we just show "Bot started"
      String cleanMsg = response;
      cleanMsg.replace("{\"status\":\"", "");
      cleanMsg.replace("\"}", "");
      lcd.print(cleanMsg.substring(0, 16)); 
      
    } else {
      // Handle Error -11 or other connection issues
      Serial.println("Error: " + String(httpResponseCode));
      lcd.setCursor(0, 0);
      lcd.print("Error Code:");
      lcd.setCursor(0, 1);
      lcd.print(String(httpResponseCode) + " (Timeout?)");
    }

    http.end();
  } else {
    lcd.clear();
    lcd.print("No WiFi Conn");
  }
}

void stopBot(String serverName,String botId) {
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
    Serial.println("Waiting for server...");
    int httpResponseCode = http.POST(httpRequestData);

    lcd.clear();
    if (httpResponseCode > 0) {
      String response = http.getString();
      
      // Print to Serial for debugging
      Serial.println("Code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
      
      // Print to LCD (Top row: Code, Bottom row: Message)
      lcd.setCursor(0, 0);
      lcd.print("HTTP Code: " + String(httpResponseCode));
      
      lcd.setCursor(0, 1);
      // Clean up the JSON string for the small screen
      // If response is {"status":"Bot started"}, we just show "Bot started"
      String cleanMsg = response;
      cleanMsg.replace("{\"status\":\"", "");
      cleanMsg.replace("\"}", "");
      lcd.print(cleanMsg.substring(0, 16)); 
      
    } else {
      // Handle Error -11 or other connection issues
      Serial.println("Error: " + String(httpResponseCode));
      lcd.setCursor(0, 0);
      lcd.print("Error Code:");
      lcd.setCursor(0, 1);
      lcd.print(String(httpResponseCode) + " (Timeout?)");
    }

    http.end();
  } else {
    lcd.clear();
    lcd.print("No WiFi Conn");
  }
}