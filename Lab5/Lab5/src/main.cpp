/*
 * Lab 5: Power-Efficient Ultrasonic Monitor
 * 
 * 3-State Power Strategy for 24h+ operation on 500mAh battery
 * 
 * States:
 *   MONITOR    - WiFi OFF, read every 4s, ~20mA (default)
 *   EVENT      - WiFi ON for upload, read every 1s, ~55mA (limited time)
 *   DEEP_SLEEP - 30s sleep, ~3mA
 * 
 * Transitions:
 *   MONITOR → EVENT:      distance < 50cm detected
 *   MONITOR → DEEP_SLEEP: distance > 50cm for 30s
 *   EVENT → MONITOR:      distance > 50cm for 10s
 *   DEEP_SLEEP → MONITOR: after 30s wake
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sleep.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>


#define WIFI_SSID "UW MPSK"
#define WIFI_PASSWORD "kbpRN5cHsMxsFXKu"

#define API_KEY "AIzaSyDoxVzcv7aVMAjzNJjTi66bSM8ItYVplaE"
#define USER_EMAIL "chenyuwennnn@gmail.com"
#define USER_PASSWORD "Chenyuwen12!"
#define DATABASE_URL "https://lab5-b32dc-default-rtdb.firebaseio.com/"

#define TRIG_PIN D0
#define ECHO_PIN D1

#define DISTANCE_THRESHOLD 50.0   
#define MONITOR_READ_INTERVAL 4000 
#define EVENT_READ_INTERVAL 1000   
#define EVENT_UPLOAD_INTERVAL 4000
#define MONITOR_TO_SLEEP_TIME 30000  
#define EVENT_TO_MONITOR_TIME 10000  
#define DEEP_SLEEP_DURATION 30     

// States
enum State {
    STATE_MONITOR = 0,
    STATE_EVENT = 1,
    STATE_DEEP_SLEEP = 2
};

RTC_DATA_ATTR int currentState = STATE_MONITOR;
RTC_DATA_ATTR int bootCount = 0;

UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl;
AsyncClientClass asyncClient(ssl);
RealtimeDatabase database;

unsigned long lastReadTime = 0;
unsigned long lastUploadTime = 0;
unsigned long farDistanceStart = 0; 
bool isFarDistance = false;

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

void firebaseCallback(AsyncResult &aResult) {
    if (aResult.isError()) {
        Serial.printf("Firebase Error: %s\n", aResult.error().message().c_str());
    }
    if (aResult.available()) {
        Serial.printf("Firebase OK: %s\n", aResult.c_str());
    }
}

// Connect WiFi
bool connectWiFi() {
    Serial.print("WiFi connecting");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(" Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println(" Failed!");
    return false;
}

void disconnectWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi OFF");
}

bool initFirebase() {
    Serial.println("Firebase init...");
    ssl.setInsecure();
    ssl.setHandshakeTimeout(5);
    
    initializeApp(asyncClient, app, getAuth(user_auth), firebaseCallback, "auth");
    app.getApp<RealtimeDatabase>(database);
    database.url(DATABASE_URL);
    
    unsigned long start = millis();
    while (!app.ready() && millis() - start < 10000) {
        app.loop();
        delay(100);
    }
    
    Serial.printf("Firebase: %s\n", app.ready() ? "Ready" : "Failed");
    return app.ready();
}

void uploadToFirebase(float distance) {
    Serial.println("Uploading to Firebase...");
    
    if (!connectWiFi()) {
        return;
    }
    
    if (!initFirebase()) {
        disconnectWiFi();
        return;
    }

    database.set<float>(asyncClient, "/Lab5/distance", distance, firebaseCallback, "dist");
    database.set<number_t>(asyncClient, "/Lab5/timestamp", number_t(millis()), firebaseCallback, "time");
    database.set<String>(asyncClient, "/Lab5/state", String("EVENT"), firebaseCallback, "state");
    
    unsigned long start = millis();
    while (millis() - start < 2000) {
        app.loop();
        delay(50);
    }
    
    disconnectWiFi();
    Serial.println("Upload complete, WiFi OFF");
}

void enterDeepSleep() {
    Serial.println("\n========================================");
    Serial.println("Entering DEEP SLEEP for 30 seconds...");
    Serial.println("========================================");
    Serial.flush();
    
    currentState = STATE_DEEP_SLEEP;
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * 1000000ULL);
    esp_deep_sleep_start();
}

const char* getStateName(int state) {
    switch (state) {
        case STATE_MONITOR: return "MONITOR";
        case STATE_EVENT: return "EVENT";
        case STATE_DEEP_SLEEP: return "DEEP_SLEEP";
        default: return "UNKNOWN";
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    bootCount++;
    
    Serial.println("\n========================================");
    Serial.println("Power-Efficient Ultrasonic Monitor");
    Serial.printf("Boot #%d | Wakeup: %d\n", bootCount, esp_sleep_get_wakeup_cause());
    Serial.println("========================================\n");
    
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("Woke from deep sleep → MONITOR");
        currentState = STATE_MONITOR;
    }
    
    Serial.printf("Current State: %s\n", getStateName(currentState));
    Serial.println("========================================\n");
    
    lastReadTime = 0;
    lastUploadTime = 0;
    farDistanceStart = 0;
    isFarDistance = false;
}

void loop() {
    unsigned long now = millis();
    float distance;
    
    switch (currentState) {
        
        case STATE_MONITOR:
            if (now - lastReadTime >= MONITOR_READ_INTERVAL) {
                distance = readDistance();
                Serial.printf("[MONITOR] Distance: %.2f cm\n", distance);
                lastReadTime = now;
                
                if (distance > 0 && distance < DISTANCE_THRESHOLD) {
                    Serial.println("*** Object detected! → EVENT ***");
                    currentState = STATE_EVENT;
                    isFarDistance = false;
                    farDistanceStart = 0;
                    lastUploadTime = 0;  
                }

                else if (distance > DISTANCE_THRESHOLD || distance < 0) {
                    if (!isFarDistance) {
                        isFarDistance = true;
                        farDistanceStart = now;
                        Serial.println("Far distance started...");
                    } else if (now - farDistanceStart >= MONITOR_TO_SLEEP_TIME) {
                        Serial.println("30s far distance → DEEP SLEEP");
                        enterDeepSleep();
                    } else {
                        Serial.printf("Far for %lu/%d ms\n", 
                                     now - farDistanceStart, MONITOR_TO_SLEEP_TIME);
                    }
                }
            }
            break;
         
        case STATE_EVENT:
            if (now - lastReadTime >= EVENT_READ_INTERVAL) {
                distance = readDistance();
                Serial.printf("[EVENT] Distance: %.2f cm\n", distance);
                lastReadTime = now;
                
                if (now - lastUploadTime >= EVENT_UPLOAD_INTERVAL) {
                    uploadToFirebase(distance);
                    lastUploadTime = now;
                }
            
                if (distance > DISTANCE_THRESHOLD || distance < 0) {
                    if (!isFarDistance) {
                        isFarDistance = true;
                        farDistanceStart = now;
                        Serial.println("Object moving away...");
                    } else if (now - farDistanceStart >= EVENT_TO_MONITOR_TIME) {
                        Serial.println("10s no object → MONITOR");
                        currentState = STATE_MONITOR;
                        isFarDistance = false;
                    } else {
                        Serial.printf("Far for %lu/%d ms\n", 
                                     now - farDistanceStart, EVENT_TO_MONITOR_TIME);
                    }
                } else {
                    isFarDistance = false;
                    farDistanceStart = 0;
                }
            }
            break;
            
        case STATE_DEEP_SLEEP:
            enterDeepSleep();
            break;
    }
    
    delay(50);
}
