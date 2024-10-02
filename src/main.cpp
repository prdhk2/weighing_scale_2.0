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

SoftwareSerial mySerial(18, 0);
float weightFloat = 0.0;

boolean resetTime = false;
unsigned long resetMillis = 0;

const int ledPin = 15;
const int buzzerPin = 2;
const int buttonPin = 4;  

int lastState = HIGH;
int currentState = LOW;

const char *ssid = "Biodigester";
const char *password = "zerowaste";

// Deklarasi fungsi
void toggleLED(int pin, int onDuration, int offDuration);
void readWeight();
void SendToServer();
void printWeight();
void beepBuzzer(int frequency, int duration);
void displayStatus(String status, bool success);
String extractNumericPart(String inputString);

unsigned long lastButtonPressMillis = 0;  // Waktu tombol terakhir ditekan
unsigned long delayDuration = 10000;

void setup() {
  Serial.begin(9600);
  mySerial.begin(4800);

  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // OLED setup
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(WHITE);
  display.setCursor(0, 0); 
  display.println("Loading...");
  display.display();

  // WiFi setup
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    toggleLED(ledPin, 100, 100);
    Serial.println("Connecting to WiFi...");
    delay(500);
  }

  Serial.println("Connected to WiFi");
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(2);
  display.println("WiFi");
  display.println("Terhubung !");
  display.display();
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
  printWeight();

  int buttonState = digitalRead(buttonPin);

  if (buttonState == LOW) {
    SendToServer();
    printWeight();

    lastButtonPressMillis = millis();

    display.clearDisplay();
    display.setCursor(0, 10);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("Mengirim Data...");
    display.display();
    
    Serial.println("Button pressed");
  }

  if (millis() - lastButtonPressMillis >= delayDuration) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("Siap Digunakan");
    display.display();
  }
}

void readWeight() {
  if (mySerial.available()) {
    String weight = mySerial.readStringUntil('\n');
    String numericPart = extractNumericPart(weight);
    weightFloat = numericPart.toFloat();

    display.clearDisplay();
    display.setCursor(0, 10);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("Berat: ");
    display.println(weightFloat, 2);
    display.display();

  }
}

String extractNumericPart(String inputString) {
  String numericPart = "";
  boolean foundNumeric = false;

  for (int i = 0; i < inputString.length(); i++) {
    char currentChar = inputString.charAt(i);

    if (isDigit(currentChar) || currentChar == '.' || currentChar == '-') {
      numericPart += currentChar;
      foundNumeric = true;
    } else if (foundNumeric) {
      break;
    }
  }

  return numericPart;
}

void SendToServer() {
  HTTPClient http;

  String postData = "weightFloat=" + String(weightFloat, 2);

  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);

    beepBuzzer(1000, 500);
    displayStatus("Data Terkirim!", true);

  } else {
    Serial.print("HTTP Error code: ");
    Serial.println(httpResponseCode);

    beepBuzzer(1000, 2000); // Bunyi lebih lama untuk error
    displayStatus("Gagal Kirim!", false);
  }
  http.end();
}

void printWeight() {
  Serial.print("Berat: ");
  Serial.println(weightFloat);
}

void toggleLED(int pin, int onDuration, int offDuration) {
  digitalWrite(pin, HIGH);
  delay(onDuration);
  digitalWrite(pin, LOW);
  delay(offDuration);
}

void beepBuzzer(int frequency, int duration) {
  digitalWrite(buzzerPin, HIGH);
  delay(duration);
  digitalWrite(buzzerPin, LOW);
}

void displayStatus(String status, bool success) {
  display.clearDisplay();
  display.setCursor(0, 20);
  display.setTextSize(2);

  if(success) {
    display.setTextColor(WHITE);
    display.println(status);
  } else {
    display.setTextColor(WHITE);
    display.println(status);
  }
  display.display();
}
