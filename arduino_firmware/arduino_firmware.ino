#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <WiFiEsp.h>

// Pin Definitions
const int BUZZER_PIN = 8;
const int LED_RED = 9;
const int LED_GREEN = 10;
const int LED_BLUE = 11;
const int CS_PIN = 4;

// Serial to ESP8266
SoftwareSerial espSerial(2, 3); // RX, TX

// Network settings
char ssid[] = "YOUR_SSID";
char pass[] = "YOUR_PASSWORD";
char server[] = "192.168.1.100"; // Example server IP
int port = 3000;

WiFiEspClient client;

// LCD Initialization (0x27 address, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Game State
unsigned long lastDrawTime = 0;
const unsigned long DRAW_INTERVAL = 7000;
bool gameActive = false;

// Hardware action placeholders
void playDrawSound() {
  // To be implemented
}

void playWinSound() {
  // To be implemented
}

void flashWinningLEDs() {
  // To be implemented
}

void updateLCD(String message) {
  // Basic implementation to show something on screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}

void saveScoreToSD(String name, String time) {
  // To be implemented
}

void connectNetwork() {
  Serial.println("Initializing ESP8266...");
  WiFi.init(&espSerial);
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ESP8266 shield not present");
    while (true); // block
  }
  
  Serial.println("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
  }
  Serial.println("Connected to Wi-Fi!");
  
  Serial.println("Connecting to server...");
  if (client.connect(server, port)) {
    Serial.println("Connected to server successfully");
    gameActive = true;
  } else {
    Serial.println("Server connection failed");
  }
}

void setup() {
  // Initialize Serial communication
  Serial.begin(9600);
  espSerial.begin(9600);

  // Configure pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // Initialize SD Card
  if (!SD.begin(CS_PIN)) {
    Serial.println("SD card initialization failed!");
  } else {
    Serial.println("SD card initialized successfully.");
  }
  
  // Seed random generator
  randomSeed(analogRead(0));

  // Connect to Network & Server
  connectNetwork();
}

void loop() {
  // Non-blocking timer for generating numbers
  if (gameActive && (millis() - lastDrawTime >= DRAW_INTERVAL)) {
    lastDrawTime = millis();
    
    // Generate a random number (0-99)
    int drawnNumber = random(0, 100);
    
    // Send to server
    if (client.connected()) {
      client.print("NUM:");
      client.println(drawnNumber);
    }
    
    updateLCD("Drawn: " + String(drawnNumber));
    playDrawSound();
  }

  // Listen for incoming messages from the server
  if (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    
    if (line.startsWith("WINNER:")) {
      gameActive = false;
      
      // Expected string format: "WINNER:Name:Time"
      int firstColon = line.indexOf(':');
      int secondColon = line.indexOf(':', firstColon + 1);
      
      if (firstColon != -1 && secondColon != -1) {
        String winnerName = line.substring(firstColon + 1, secondColon);
        String winTime = line.substring(secondColon + 1);
        
        updateLCD("WIN: " + winnerName);
        playWinSound();
        flashWinningLEDs();
        saveScoreToSD(winnerName, winTime);
      }
    }
  }
}
