#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char *serverUrl = "https://rizqisemesta.com/taman-jatisari/timbangan.php";

SoftwareSerial mySerial(17, 0);
float weightFloat = 0.0;
float lastWeights[5]; // Array untuk menyimpan pembacaan berat terakhir
int weightCount[5]; // Hitung kemunculan setiap berat
int weightIndex = 0; // Indeks saat ini untuk menyimpan berat
float lockedWeight = 0.0; // Berat yang terkunci

const int ledPin = 15;
const int buzzerPin = 2;
const int buttonPin = 4;  

const char *ssid = "RIZQI SEMESTA";
const char *password = "z3r0w4sT3";

unsigned long averageTimeMilis = 0;
unsigned long averageDuration = 3000;

unsigned long lastButtonPressMillis = 0;  
unsigned long delayDuration = 10000;

// Function declarations
void toggleLED(int pin, int onDuration, int offDuration);
void readWeight();
void SendToServer(float weight);
void beepBuzzer(int duration);
void displayStatus(String status, bool success);
String extractNumericPart(String inputString);
bool isButtonPressed();
void checkWeightLock();

void setup() {
    Serial.begin(9600);
    mySerial.begin(4800);
    pinMode(ledPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);

    // Setup OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
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
        toggleLED(ledPin, 100, 100);
        Serial.println("Connecting to WiFi...");
        delay(500);
        retryCount++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi after several attempts.");
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
    if (WiFi.status() != WL_CONNECTED) {
        toggleLED(ledPin, 100, 100);
        Serial.println("WiFi lost, reconnecting...");
        WiFi.reconnect();
    } else {
        toggleLED(ledPin, 1000, 1000);
    }

    readWeight();
    checkWeightLock(); // Periksa berat yang terkunci

    if (isButtonPressed()) {
        if (lockedWeight > 0) {
            SendToServer(lockedWeight); // Kirim berat yang terkunci
            lastButtonPressMillis = millis();
            display.clearDisplay();
            display.setCursor(0, 10);
            display.setTextSize(2);
            display.println("Mengirim Data...");
            display.display();
        } else {
            lastButtonPressMillis = millis();
            display.clearDisplay();
            display.setCursor(0, 10);
            display.setTextSize(2);
            display.println("Berat tidak stabil");
            display.display();
        }
    }

    if (millis() - lastButtonPressMillis >= delayDuration && lockedWeight >= 0) {
        display.clearDisplay();
        display.setCursor(0, 10);
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.println("Berat: ");
        display.println(lockedWeight, 2);
        display.display();
    } 
}

void readWeight() {
    if (mySerial.available()) {
        String weight = mySerial.readStringUntil('\n');
        String numericPart = extractNumericPart(weight);
        weightFloat = numericPart.toFloat();

        // Simpan berat dan hitung kemunculannya
        lastWeights[weightIndex] = weightFloat;
        weightCount[weightIndex] = 1;

        // Periksa untuk duplikat
        for (int i = 0; i < 5; i++) {
            if (lastWeights[i] == weightFloat) {
                weightCount[i]++;
            }
        }

        // Pindah ke indeks berikutnya
        weightIndex = (weightIndex + 1) % 5; // Kembali ke awal
    }
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

void checkWeightLock() {
    for (int i = 0; i < 5; i++) {
        if (weightCount[i] > 3) {
            lockedWeight = lastWeights[i]; 
            break;
        } else {
            lockedWeight = 0.0;
        }
    }
}

void SendToServer(float weight) {
    HTTPClient http;

    String postData = "lockedWeight=" + String(weight, 2); // Mengirim berat yang terkunci atau saat ini

    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(postData); // Mengirim data

    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        Serial.println(response);
        beepBuzzer(500);
        displayStatus("Data Terkirim!", true);
    } else {
        Serial.print("HTTP Error code: ");
        Serial.println(httpResponseCode);
        beepBuzzer(2000);
        displayStatus("Gagal Kirim!", false);
    }
    http.end();
}

void toggleLED(int pin, int onDuration, int offDuration) {
    digitalWrite(pin, HIGH);
    delay(onDuration);
    digitalWrite(pin, LOW);
    delay(offDuration);
}

void beepBuzzer(int duration) {
    digitalWrite(buzzerPin, HIGH);
    delay(duration);
    digitalWrite(buzzerPin, LOW);
}

void displayStatus(String status, bool success) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println(status);
    display.display();
}

bool isButtonPressed() {
    static unsigned long lastDebounceTime = 0;
    static int lastButtonState = HIGH;
    static int lastState = HIGH;
    int reading = digitalRead(buttonPin);

    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) {
        if (reading != lastState) {
            lastState = reading;
            return (lastState == LOW);
        }
    }
    lastButtonState = reading;
    return false;
}
