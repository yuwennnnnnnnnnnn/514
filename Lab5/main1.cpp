/*
 * ============================================================================
 * LAB 5: POWER CONSUMPTION MODES DEMONSTRATION
 * ============================================================================
 * 
 * This program demonstrates five different power consumption modes on an ESP32.
 * Each mode runs for 10 seconds, then automatically transitions to the next mode.
 * The cycle repeats indefinitely.
 * 
 * Power Modes (in order):
 *   1. DEEP SLEEP      - Lowest power consumption, timer wake-up
 *   2. IDLE            - Awake but minimal processing
 *   3. ULTRASONIC      - HC-SR04 sensor readings every 500ms
 *   4. WiFi ONLY       - WiFi active, monitoring connection quality
 *   5. FULL OPERATION  - Ultrasonic + WiFi + Firebase data transmission
 * 
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sleep.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>

// ============================================================================
// CONFIGURATION SECTION
// ============================================================================

// WiFi Credentials
#define WIFI_SSID "UW MPSK"
#define WIFI_PASSWORD "kbpRN5cHsMxsFXKu"

// Firebase Credentials
#define API_KEY "AIzaSyDoxVzcv7aVMAjzNJjTi66bSM8ItYVplaE"
#define USER_EMAIL "chenyuwennnn@gmail.com"
#define USER_PASSWORD "Chenyuwen12!"
#define DATABASE_URL "https://lab5-b32dc-default-rtdb.firebaseio.com/"

// Ultrasonic Sensor Configuration (HC-SR04)
#define TRIG_PIN D0
#define ECHO_PIN D1
#define ULTRASONIC_TIMEOUT_US 30000  // 30ms timeout for pulse reading
#define SOUND_SPEED_CM_US 0.0343     // Speed of sound in cm/microsecond

// Timing Configuration
#define MODE_DURATION_MS 10000        // Duration of each mode in milliseconds
#define DEEP_SLEEP_DURATION_US 10000000  // 10 seconds in microseconds
#define ULTRASONIC_READ_INTERVAL 500  // Read ultrasonic every 500ms
#define FIREBASE_SEND_INTERVAL 2000   // Send to Firebase every 2 seconds

// ============================================================================
// POWER MODE DEFINITIONS
// ============================================================================

enum PowerMode {
    MODE_DEEP_SLEEP = 0,                    // Lowest power: CPU off, timer wake-up
    MODE_IDLE = 1,                          // CPU on, no processing
    MODE_ULTRASONIC = 2,                    // Sensor readings only
    MODE_WIFI_ONLY = 3,                     // WiFi active, no sensors
    MODE_ULTRASONIC_WIFI_FIREBASE = 4,      // Full operation: sensors + WiFi + cloud
    MODE_COUNT = 5                          // Total number of modes
};

// RTC Memory - Persists across deep sleep cycles
RTC_DATA_ATTR int currentMode = MODE_DEEP_SLEEP;
RTC_DATA_ATTR int bootCount = 0;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Firebase Configuration Objects
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client1, ssl_client2;
using AsyncClient = AsyncClientClass;
AsyncClient async_client1(ssl_client1), async_client2(ssl_client2);
RealtimeDatabase Database;
AsyncResult dbResult;

// Timing Variables
unsigned long modeStartTime = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

// Mode Functions
void runDeepSleep();
void runIdleMode();
void runUltrasonicMode();
void runWiFiOnlyMode();
void runUltrasonicWiFiFirebaseMode();

// Sensor Functions
float readUltrasonic();

// WiFi Functions
void setupWiFi();
void stopWiFi();

// Firebase Functions
void setupFirebase();
void processData(AsyncResult &aResult);

// Utility Functions
const char* getModeName(PowerMode mode);

// ============================================================================
// SETUP FUNCTION - Runs once at startup
// ============================================================================

void setup() {
    // Initialize Serial Communication
    Serial.begin(115200);
    delay(1000);  // Allow serial to initialize
    
    // Increment boot counter
    bootCount++;
    
    // Print startup information
    Serial.println();
    Serial.println("==================== STARTUP ====================");
    Serial.printf("Boot Count: %d\n", bootCount);
    Serial.printf("Wakeup Cause: %d\n", esp_sleep_get_wakeup_cause());
    
    // Configure Ultrasonic Sensor Pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    // Handle mode transition after deep sleep wake-up
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("Woke up from deep sleep timer");
        currentMode = (currentMode + 1) % MODE_COUNT;
    }
    
    Serial.printf("Current Mode: %d - %s\n", currentMode, getModeName((PowerMode)currentMode));
    Serial.println("===============================================\n");
    
    // Record mode start time
    modeStartTime = millis();
    
    // Initialize mode-specific hardware
    if (currentMode == MODE_DEEP_SLEEP) {
        // Deep sleep mode: enter sleep immediately
        runDeepSleep();
    }
    
    if (currentMode == MODE_WIFI_ONLY || currentMode == MODE_ULTRASONIC_WIFI_FIREBASE) {
        // WiFi-dependent modes: initialize WiFi
        setupWiFi();
    }
    
    if (currentMode == MODE_ULTRASONIC_WIFI_FIREBASE) {
        // Full operation mode: initialize Firebase
        setupFirebase();
    }
}

// ============================================================================
// MAIN LOOP - Executes repeatedly
// ============================================================================

void loop() {
    // Calculate elapsed time in current mode
    unsigned long elapsed = millis() - modeStartTime;
    
    // Check if mode duration has expired
    if (elapsed >= MODE_DURATION_MS) {
        Serial.println();
        Serial.println("--- Mode Duration Complete ---");
        
        // Cleanup: Stop WiFi if currently active
        if (currentMode == MODE_WIFI_ONLY || currentMode == MODE_ULTRASONIC_WIFI_FIREBASE) {
            stopWiFi();
        }
        
        // Transition to next mode
        currentMode = (currentMode + 1) % MODE_COUNT;
        Serial.println();
        Serial.printf("Switching to Mode: %d - %s\n", currentMode, getModeName((PowerMode)currentMode));
        Serial.println("===============================================\n");
        
        // Initialize hardware for new mode
        if (currentMode == MODE_DEEP_SLEEP) {
            // Deep sleep: enter immediately
            runDeepSleep();
        }
        
        if (currentMode == MODE_WIFI_ONLY || currentMode == MODE_ULTRASONIC_WIFI_FIREBASE) {
            // WiFi modes: initialize WiFi connection
            setupWiFi();
        }
        
        if (currentMode == MODE_ULTRASONIC_WIFI_FIREBASE) {
            // Full operation mode: initialize Firebase
            setupFirebase();
        }
        
        // Reset mode timer
        modeStartTime = millis();
    }
    
    // Execute current mode logic
    switch (currentMode) {
        case MODE_IDLE:
            runIdleMode();
            break;
            
        case MODE_ULTRASONIC:
            runUltrasonicMode();
            break;
            
        case MODE_WIFI_ONLY:
            runWiFiOnlyMode();
            break;
            
        case MODE_ULTRASONIC_WIFI_FIREBASE:
            runUltrasonicWiFiFirebaseMode();
            break;
            
        default:
            break;
    }
    
    // Small delay to prevent CPU saturation
    delay(100);
}

// ============================================================================
// MODE IMPLEMENTATIONS
// ============================================================================

// MODE 0: DEEP SLEEP
// Lowest power consumption mode. CPU is off and only woken by timer.
void runDeepSleep() {
    Serial.println("\n========== MODE 0: DEEP SLEEP ==========");
    Serial.println("Entering deep sleep for 10 seconds...");
    Serial.println("CPU OFF - Lowest power consumption");
    Serial.flush();
    
    // Configure timer-based wake-up
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US);
    
    // Enter deep sleep (does not return)
    esp_deep_sleep_start();
    
    // Execution never reaches this point
}

// MODE 1: IDLE
// CPU is powered on but performing minimal operations.
void runIdleMode() {
    static unsigned long lastPrint = 0;
    
    // Print status every 1 second
    if (millis() - lastPrint >= 1000) {
        Serial.println("\n========== MODE 1: IDLE ==========");
        Serial.println("CPU awake, minimal processing");
        unsigned long timeRemaining = (MODE_DURATION_MS - (millis() - modeStartTime)) / 1000;
        Serial.printf("Time Remaining: %lu seconds\n", timeRemaining);
        lastPrint = millis();
    }
    
    // Minimal CPU usage - just idle wait
}

// MODE 2: ULTRASONIC
// Sensor readings every 500ms. WiFi inactive.
void runUltrasonicMode() {
    static unsigned long lastReading = 0;
    
    // Take ultrasonic reading every 500ms
    if (millis() - lastReading >= ULTRASONIC_READ_INTERVAL) {
        Serial.println("\n========== MODE 2: ULTRASONIC ==========");
        float distance = readUltrasonic();
        Serial.printf("Distance: %.2f cm\n", distance);
        
        unsigned long timeRemaining = (MODE_DURATION_MS - (millis() - modeStartTime)) / 1000;
        Serial.printf("Time Remaining: %lu seconds\n", timeRemaining);
        
        lastReading = millis();
    }
}

// MODE 3: WiFi ONLY
// WiFi is active and monitoring connection quality. No sensor readings.
void runWiFiOnlyMode() {
    static unsigned long lastPrint = 0;
    
    // Print status every 1 second
    if (millis() - lastPrint >= 1000) {
        Serial.println("\n========== MODE 3: WiFi ONLY ==========");
        Serial.println("WiFi Active - No Sensor Readings");
        
        // Display WiFi connection status
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi Status: CONNECTED");
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        } else {
            Serial.println("WiFi Status: DISCONNECTED");
        }
        
        unsigned long timeRemaining = (MODE_DURATION_MS - (millis() - modeStartTime)) / 1000;
        Serial.printf("Time Remaining: %lu seconds\n", timeRemaining);
        
        lastPrint = millis();
    }
}

// MODE 4: FULL OPERATION
// Ultrasonic sensor readings + WiFi + Firebase cloud sync.
// This is the highest power consumption mode.
void runUltrasonicWiFiFirebaseMode() {
    static unsigned long lastReading = 0;
    
    // Maintain Firebase async operations
    app.loop();
    
    // Send sensor data to Firebase every 2 seconds
    if (millis() - lastReading >= FIREBASE_SEND_INTERVAL) {
        Serial.println("\n========== MODE 4: FULL OPERATION ==========");
        Serial.println("Ultrasonic + WiFi + Firebase");
        
        // Read distance from ultrasonic sensor
        float distance = readUltrasonic();
        Serial.printf("Distance: %.2f cm\n", distance);
        
        // Transmit data to Firebase
        if (app.ready()) {
            // Send distance value
            String distancePath = "/Lab5/ultrasonic/distance";
            Database.set<float>(async_client1, distancePath.c_str(), distance, processData, "SetDistance");
            
            // Send timestamp
            String timestampPath = "/Lab5/ultrasonic/timestamp";
            Database.set<number_t>(async_client1, timestampPath.c_str(), number_t(millis()), dbResult);
            
            Serial.println("Firebase: Data transmitted");
        } else {
            Serial.println("Firebase: Initializing...");
        }
        
        // Display network metrics
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        
        unsigned long timeRemaining = (MODE_DURATION_MS - (millis() - modeStartTime)) / 1000;
        Serial.printf("Time Remaining: %lu seconds\n", timeRemaining);
        
        lastReading = millis();
    }
    
    // Process Firebase asynchronous results
    processData(dbResult);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Read distance from HC-SR04 ultrasonic sensor
// Returns distance in cm, or -1.0 if no echo received
float readUltrasonic() {
    // Step 1: Prepare for measurement - clear trigger pin
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    // Step 2: Send 10us trigger pulse
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    // Step 3: Measure echo pulse duration
    // Timeout set to 30ms (exceeds max distance ~5 meters)
    long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
    
    // Step 4: Return error code if no echo received
    if (duration == 0) {
        return -1.0;
    }
    
    // Step 5: Calculate distance
    // Formula: distance = (duration * sound_speed) / 2
    // Factor of 2 accounts for round-trip travel
    float distance = (duration * SOUND_SPEED_CM_US) / 2.0;
    
    return distance;
}

// Initialize WiFi connection
// Attempts to connect for up to 15 seconds
void setupWiFi() {
    Serial.println("Initializing WiFi...");
    
    // Set WiFi mode to Station (client)
    WiFi.mode(WIFI_STA);
    
    // Begin connection attempt
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("Connecting to WiFi");
    
    // Wait for connection with timeout
    int attempts = 0;
    const int MAX_ATTEMPTS = 30;  // ~15 seconds with 500ms delays
    
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
        Serial.print(".");
        delay(500);
        attempts++;
    }
    
    // Report connection status
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi: SUCCESS");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("WiFi: FAILED (Timeout)");
    }
}

// Disable WiFi and power down radio
void stopWiFi() {
    Serial.println("Disabling WiFi...");
    
    // Disconnect and turn off WiFi radio
    WiFi.disconnect(true);  // true = turn off radio
    WiFi.mode(WIFI_OFF);
    
    Serial.println("WiFi: Disabled");
}

// Initialize Firebase connection
// Configures SSL security and authentication
void setupFirebase() {
    Serial.println("Initializing Firebase...");
    
    // Configure SSL/TLS security
    ssl_client1.setInsecure();  // Accept any certificate
    ssl_client2.setInsecure();
    ssl_client1.setHandshakeTimeout(5);  // 5 second timeout
    ssl_client2.setHandshakeTimeout(5);
    
    // Initialize Firebase app with authentication
    initializeApp(async_client1, app, getAuth(user_auth), processData, "authTask");
    
    // Get database reference and set URL
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
    
    Serial.println("Firebase: Initialized");
}

// Process Firebase asynchronous operation results
// Handles events, debug messages, errors, and data responses
void processData(AsyncResult &aResult) {
    // Skip if no result available
    if (!aResult.isResult())
        return;

    // Log event messages
    if (aResult.isEvent())
        Firebase.printf("[EVENT] Task: %s | Message: %s | Code: %d\n",
                       aResult.uid().c_str(),
                       aResult.eventLog().message().c_str(),
                       aResult.eventLog().code());

    // Log debug information
    if (aResult.isDebug())
        Firebase.printf("[DEBUG] Task: %s | Message: %s\n",
                       aResult.uid().c_str(),
                       aResult.debug().c_str());

    // Log error information
    if (aResult.isError())
        Firebase.printf("[ERROR] Task: %s | Message: %s | Code: %d\n",
                       aResult.uid().c_str(),
                       aResult.error().message().c_str(),
                       aResult.error().code());

    // Log data payload if available
    if (aResult.available())
        Firebase.printf("[DATA] Task: %s | Payload: %s\n",
                       aResult.uid().c_str(),
                       aResult.c_str());
}

// Return human-readable name for power mode
const char* getModeName(PowerMode mode) {
    switch (mode) {
        case MODE_DEEP_SLEEP:
            return "Deep Sleep";
        case MODE_IDLE:
            return "Idle";
        case MODE_ULTRASONIC:
            return "Ultrasonic Only";
        case MODE_WIFI_ONLY:
            return "WiFi Only";
        case MODE_ULTRASONIC_WIFI_FIREBASE:
            return "Ultrasonic + WiFi + Firebase";
        default:
            return "Unknown";
    }
}

// ============================================================================
// END OF FILE
// ============================================================================