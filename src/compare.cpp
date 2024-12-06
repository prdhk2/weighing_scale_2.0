#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Konfigurasi OLED
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// URL Server
const char *serverUrl = "https://rizqisemesta.com/taman-jatisari/timbangan.php";

// HardwareSerial untuk komunikasi timbangan
HardwareSerial mySerial(1); // UART1 ESP32 (GPIO17 RX, GPIO16 TX)
float weightFloat = 0.0;

// Pin dan variabel kontrol
const int ledPin = 15;
const int buzzerPin = 2;
const int buttonPin = 4;
const char *ssid = "Biodigester";
const char *password = "z3r0w4sT3";

unsigned long lastButtonPressMillis = 0;  
unsigned long lastLedMillis = 0;
unsigned long lastBuzzerMillis = 0;
unsigned long delayDuration = 10000;  // Durasi tampilan berat
unsigned long ledBlinkInterval = 1000; // Interval blink LED
unsigned long buzzerDuration = 0;  // Durasi beep buzzer

// Function declarations
void reconnectWiFi();
float readWeight();
void sendToServer(float weight);
void toggleLED();
void beepBuzzer();
void displayStatus(String status, bool success);
String extractNumericPart(String inputString);
bool isButtonPressed();

void setup() {
    Serial.begin(9600);
    mySerial.begin(4800, SERIAL_8N1, 17, 16); // Pin RX/TX untuk timbangan
    pinMode(ledPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);

    // Setup OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        while (1); // Berhenti jika OLED gagal
    }
    display.clearDisplay();
    display.setTextSize(2); 
    display.setTextColor(WHITE);
    display.setCursor(0, 0); 
    display.println("Loading...");
    display.display();

    // Setup WiFi
    WiFi.begin(ssid, password);
    int retryCount = 0;
    const int maxRetries = 10;

    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
        toggleLED();
        Serial.println("Connecting to WiFi...");
        delay(500);
        retryCount++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi.");
    } else {
        Serial.println("Connected to WiFi");
        display.clearDisplay();
        display.setCursor(0, 10);
        display.setTextSize(2);
        display.println("WiFi Terhubung!");
        display.display();
    }
}

void loop() {
    reconnectWiFi();

    // Baca berat dari timbangan
    weightFloat = readWeight();

    // Kirim data jika tombol ditekan
    if (isButtonPressed()) {
        if (weightFloat > 0) {
            sendToServer(weightFloat); 
            lastButtonPressMillis = millis();
        } else {
            lastButtonPressMillis = millis();
            displayStatus("Berat tidak stabil", false);
        }
    }

    // Tampilkan berat secara periodik
    if (millis() - lastButtonPressMillis >= delayDuration) {
        display.clearDisplay();
        display.setCursor(0, 10);
        display.setTextSize(2);
        display.println("Berat:");
        display.println(weightFloat, 2); 
        display.display();
    }

    toggleLED();
    beepBuzzer();
}

void reconnectWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            toggleLED();
            delay(500);
        }
    }
}

float readWeight() {
    if (mySerial.available()) {
        String weight = mySerial.readStringUntil('\n');
        String numericPart = extractNumericPart(weight);
        return numericPart.toFloat();
    }
    return 0.0; // Jika tidak ada data
}

String extractNumericPart(String inputString) {
    String numericPart = "";
    bool foundNumeric = false;
    bool foundDecimal = false;
    bool foundNegative = false;

    inputString.trim();

    for (int i = 0; i < inputString.length(); i++) {
        char currentChar = inputString.charAt(i);

        if (isdigit(currentChar)) {
            numericPart += currentChar;
            foundNumeric = true;
        } else if (currentChar == '.' && !foundDecimal && foundNumeric) {
            numericPart += currentChar;
            foundDecimal = true;
        } else if (currentChar == '-' && !foundNumeric && !foundNegative) {
            numericPart += currentChar;
            foundNegative = true;
        } else if (foundNumeric) {
            break;
        }
    }

    return numericPart;
}

void sendToServer(float weight) {
    HTTPClient http;

    String postData = "lockedWeight=" + String(weight, 2);

    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        Serial.println(response);
        buzzerDuration = 500;
        displayStatus("Data Terkirim!", true);
    } else {
        Serial.print("HTTP Error code: ");
        Serial.println(httpResponseCode);
        buzzerDuration = 2000;
        displayStatus("Gagal Kirim!", false);
    }
    http.end();
}

void toggleLED() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastLedMillis >= ledBlinkInterval) {
        lastLedMillis = currentMillis;
        digitalWrite(ledPin, !digitalRead(ledPin)); // Toggle LED
    }
}

void beepBuzzer() {
    static bool isBuzzerOn = false;
    unsigned long currentMillis = millis();
    if (buzzerDuration > 0) {
        if (!isBuzzerOn) {
            digitalWrite(buzzerPin, HIGH);
            isBuzzerOn = true;
            lastBuzzerMillis = currentMillis;
        } else if (currentMillis - lastBuzzerMillis >= buzzerDuration) {
            digitalWrite(buzzerPin, LOW);
            buzzerDuration = 0;
            isBuzzerOn = false;
        }
    }
}

void displayStatus(String status, bool success) {
    display.fillRect(0, 20, SCREEN_WIDTH, 20, BLACK); // Clear area status
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.setTextColor(success ? WHITE : BLACK);
    display.println(status);
    display.display();
}

bool isButtonPressed() {
    static unsigned long lastDebounceTime = 0;
    static bool buttonState = HIGH;
    int reading = digitalRead(buttonPin);

    if (reading == LOW && buttonState == HIGH && millis() - lastDebounceTime > 50) {
        lastDebounceTime = millis();
        buttonState = LOW;
        return true;
    } else if (reading == HIGH) {
        buttonState = HIGH;
    }
    return false;
}
