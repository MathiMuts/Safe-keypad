// Vault code
const char vaultCode[] = "4567";
// Wrong penalty in seconden
const int penaltyTime = 10;
// Pins en mappings
const int probePins[3] = {12, 13, 14};
const int testPins[4] = {15, 16, 17, 5};
const int lockPins[2] = {32, 33};
#define unlockPin 26  //FIXME
#define I2C_SDA 22
#define I2C_SCL 21
// Mapping of test and probe pin combinations to keypad characters
const char keypadMappings[4][3] = {
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'},
  {'1', '2', '3'}
};

const char* ssid = "Meersschaut Smart";
const char* password = "Mjcmss2112";

const char* ntfyServer = "https://ntfy.sh/";
const char* ntfyKey = "Vault-ntfy";
String notificationMessage = "Hello from ESP32!";

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_PCF8574.h>

// Initialize the LCD with the I2C address
LiquidCrystal_PCF8574 lcd(0x27);

String code_attempt = "";
char lastKey = '\0';
unsigned long lastKeyPressTime = 0;

// Delay between identical key presses (in milliseconds)
const unsigned long debounceDelay = 300;

void displayCodePrompt(String code_attempt) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Voer code in:");
  lcd.setCursor(0, 1);
  lcd.print(code_attempt);
}

void displayCooldown(int t) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Foute code!");
  if (t >= 10) {
    lcd.setCursor(13, 1);
  } else {
    lcd.setCursor(14, 1);
  }

  lcd.print(t);
  lcd.print("s");
  delay(1000);
}

void sendNotification(String message, String title, String icon) {
  HTTPClient http;
  String url = String(ntfyServer) + String(ntfyKey);
  http.begin(url);
  http.addHeader("Title", title);
  http.addHeader("Tags", icon);

  int httpCode = http.POST(message);

  // Check for successful request
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("HTTP response code: ");
    Serial.println(httpCode);
    Serial.print("Response payload: ");
    Serial.println(payload);
    lcd.setCursor(0, 1);
    lcd.print("Ping succesful!");
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(http.errorToString(httpCode).c_str());
    lcd.setCursor(0, 1);
    lcd.print("Ping failed!");
  }
  http.end();
}

bool keypadHandler() {
  for (int t = 0; t < 4; t++) {
    // Set the current test pin HIGH and others LOW
    for (int i = 0; i < 4; i++) {
      digitalWrite(testPins[i], i == t ? HIGH : LOW);
    }
    
    // Check each probe pin
    for (int p = 0; p < 3; p++) {
      if (digitalRead(probePins[p]) == HIGH) {
        char keypadChar = keypadMappings[t][p];
        
        // Check if the same key is pressed within debounceDelay time
        unsigned long currentMillis = millis();
        if (keypadChar == lastKey && currentMillis - lastKeyPressTime < debounceDelay) {
          continue;  // Ignore this key press
        }

        // Update last key press and time
        lastKey = keypadChar;
        lastKeyPressTime = currentMillis;
        // Handle different keypad inputs
        if (keypadChar == '#') {
          // Check the code
          if (code_attempt.equals(vaultCode)) {
            code_attempt = "";
            return true;
          } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Foute code!");
            sendNotification("Iemand probeerde de Vault te openen, maar is hier niet in geslaagd. De gefaalde poging is: [" + code_attempt + "].", "Kraakpoging!", "warning");
            code_attempt = "";
            for (int t = penaltyTime; t > 0; t--) {
              displayCooldown(t);
            }
            displayCodePrompt(code_attempt);
            return false;
          }
        } else if (keypadChar == '*') {
          // Delete last character from code attempt
          if (code_attempt.length() > 0) {
            code_attempt.remove(code_attempt.length() - 1);
            displayCodePrompt(code_attempt);
          }
        } else {
          // Append character to code attempt
          code_attempt += keypadChar;
          displayCodePrompt(code_attempt);
        }
        
        // Print to Serial for debugging
        Serial.print("Key pressed: ");
        Serial.println(keypadChar);
      }
    }
  }
  
  return false; // Default return if no valid key press detected
}

void setupWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Verbinden WiFi..");
  }

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Verbonden!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(1000);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("Setup start");
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize LCD
  lcd.begin(16, 2);
  lcd.setBacklight(255);

  // Setup WiFI
  setupWiFi(); 

  displayCodePrompt(code_attempt);

  for (int i = 0; i < 3; i++) {
    pinMode(probePins[i], INPUT);
  }
  for (int i = 0; i < 4; i++) {
    pinMode(testPins[i], OUTPUT);
  }
  // Set bridge pins as input/output
  pinMode(lockPins[0], OUTPUT);
  digitalWrite(lockPins[0], HIGH);
  pinMode(lockPins[1], INPUT);

  pinMode(unlockPin, OUTPUT);
  digitalWrite(unlockPin, LOW);
  Serial.println("Setup completed");
}

void loop() {
  bool lockState = keypadHandler();

  if (lockState == true) {
    digitalWrite(unlockPin, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Vault opent!");
    bool doorState = digitalRead(lockPins[1]);
    while (doorState == true) {
      delay(100);
      doorState = digitalRead(lockPins[1]);
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Vault Geopend!");
    sendNotification("Iemand heeft de vault opengemaakt.", "Vault geopend!", "unlock");
    digitalWrite(unlockPin, LOW);
    while (doorState == false) {
      delay(100);
      doorState = digitalRead(lockPins[1]);  
    }
    displayCodePrompt(code_attempt);
  }
  delay(50);
}
