/*
 * Lab 5: Ultrasonic Sensor Firebase Transmission at Different Rates
 * 
 * Transmits ultrasonic sensor data to Firebase at 5 different rates:
 *   Mode 0: 2 Hz (every 500ms)
 *   Mode 1: 1 Hz (every 1000ms)
 *   Mode 2: 0.5 Hz (every 2000ms)
 *   Mode 3: 0.333 Hz (every 3000ms)
 *   Mode 4: 0.25 Hz (every 4000ms)
 * 
 * Each mode runs for 10 seconds before switching to the next.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>

// WiFi Credentials
#define WIFI_SSID "UW MPSK"
#define WIFI_PASSWORD "kbpRN5cHsMxsFXKu"

// Firebase Credentials
#define API_KEY "AIzaSyDoxVzcv7aVMAjzNJjTi66bSM8ItYVplaE"
#define USER_EMAIL "chenyuwennnn@gmail.com"
#define USER_PASSWORD "Chenyuwen12!"
#define DATABASE_URL "https://lab5-b32dc-default-rtdb.firebaseio.com/"

// Ultrasonic Sensor Pins
#define TRIG_PIN D0
#define ECHO_PIN D1

// Mode Configuration
#define MODE_DURATION 10000  // 10 seconds per mode
#define NUM_MODES 5

// Transmission intervals in milliseconds
const unsigned long TX_INTERVALS[NUM_MODES] = {500, 1000, 2000, 3000, 4000};
const char* MODE_NAMES[NUM_MODES] = {"2 Hz", "1 Hz", "0.5 Hz", "0.333 Hz", "0.25 Hz"};

// Firebase objects
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl;
AsyncClientClass asyncClient(ssl);
RealtimeDatabase database;

// State variables
int currentMode = 0;
unsigned long modeStartTime = 0;
unsigned long lastTxTime = 0;
bool wifiConnected = false;
bool firebaseReady = false;

// Read ultrasonic sensor distance
float readDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) return -1.0;
    
    return (duration * 0.0343) / 2.0;
}

// Firebase callback
void firebaseCallback(AsyncResult &aResult) {
    if (aResult.isError()) {
        Serial.printf("Firebase Error: %s\n", aResult.error().message().c_str());
    }
    if (aResult.available()) {
        Serial.printf("Firebase OK: %s\n", aResult.c_str());
    }
}

// Connect to WiFi
void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\nWiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWiFi Failed!");
    }
}

// Initialize Firebase
void initFirebase() {
    Serial.println("Initializing Firebase...");
    
    ssl.setInsecure();
    ssl.setHandshakeTimeout(5);
    
    initializeApp(asyncClient, app, getAuth(user_auth), firebaseCallback, "auth");
    app.getApp<RealtimeDatabase>(database);
    database.url(DATABASE_URL);
    
    // Wait for Firebase to authenticate
    unsigned long start = millis();
    while (!app.ready() && millis() - start < 15000) {
        app.loop();
        delay(100);
    }
    
    firebaseReady = app.ready();
    Serial.printf("Firebase: %s\n", firebaseReady ? "Ready" : "Failed");
}

// Send data to Firebase
void sendToFirebase(float distance, int mode) {
    if (!app.ready()) {
        Serial.println("Firebase not ready");
        return;
    }
    
    // Create path with mode info
    String path = "/Lab5/mode" + String(mode) + "/distance";
    database.set<float>(asyncClient, path.c_str(), distance, firebaseCallback, "send");
    
    // Also update latest reading
    database.set<float>(asyncClient, "/Lab5/latest/distance", distance, firebaseCallback, "latest");
    database.set<number_t>(asyncClient, "/Lab5/latest/timestamp", number_t(millis()), firebaseCallback, "time");
    database.set<number_t>(asyncClient, "/Lab5/latest/mode", number_t(mode), firebaseCallback, "mode");
}

void setup() {
    Serial.begin(115200);
    
    // Wait for serial connection
    delay(3000);
    
    // Print multiple times to ensure visibility
    Serial.println();
    Serial.println();
    Serial.println("========================================");
    Serial.println("========================================");
    Serial.println("Lab 5: Firebase Transmission Rate Test");
    Serial.println("========================================");
    Serial.println("========================================");
    Serial.println();
    Serial.flush();
    
    // Setup ultrasonic pins
    Serial.println("Setting up ultrasonic sensor...");
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    Serial.println("Ultrasonic sensor ready.");
    Serial.flush();
    
    // Test ultrasonic immediately
    float testDist = readDistance();
    Serial.printf("Test reading: %.2f cm\n", testDist);
    Serial.flush();
    
    // Connect WiFi and Firebase
    Serial.println("\nConnecting to WiFi...");
    Serial.flush();
    connectWiFi();
    
    if (wifiConnected) {
        Serial.println("\nSetting up Firebase...");
        Serial.flush();
        initFirebase();
    }
    
    modeStartTime = millis();
    lastTxTime = 0;
    
    Serial.println();
    Serial.printf("Starting Mode 0: %s\n", MODE_NAMES[0]);
    Serial.println("========================================");
    Serial.println();
    Serial.flush();
}

void loop() {
    // Keep Firebase alive
    app.loop();
    
    unsigned long now = millis();
    unsigned long modeElapsed = now - modeStartTime;
    
    // Check if mode duration expired
    if (modeElapsed >= MODE_DURATION) {
        currentMode = (currentMode + 1) % NUM_MODES;
        modeStartTime = now;
        lastTxTime = 0;
        
        Serial.println();
        Serial.println("========================================");
        Serial.printf("Switching to Mode %d: %s\n", currentMode, MODE_NAMES[currentMode]);
        Serial.println("========================================");
        Serial.println();
        Serial.flush();
    }
    
    // Get current transmission interval
    unsigned long txInterval = TX_INTERVALS[currentMode];
    
    // Check if time to transmit
    if (now - lastTxTime >= txInterval) {
        float distance = readDistance();
        
        Serial.printf("[Mode %d - %s] Distance: %.2f cm", 
                     currentMode, MODE_NAMES[currentMode], distance);
        Serial.flush();
        
        if (firebaseReady && WiFi.status() == WL_CONNECTED) {
            sendToFirebase(distance, currentMode);
            Serial.println(" -> Sent!");
        } else {
            Serial.println(" -> No Firebase");
        }
        Serial.flush();
        
        lastTxTime = now;
    }
    
    delay(10);
}
