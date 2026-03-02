#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/* ========= BLE Settings ========= */
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-123456789abc"
#define DEVICE_NAME         "MuseumCounter"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

/* ========= IR Sensor Pins ========= */
const int irPin1 = 2;   // Sensor A
const int irPin2 = 3;   // Sensor B

int in_count = 0;
int out_count = 0;
int current_count = 0;

const unsigned long timeout = 300;       // detection window (ms)
const unsigned long debounceDelay = 200;  // debounce after detection (ms)
const unsigned long releaseTimeout = 2000; // max wait for sensor release (ms)

void updateCount();
void sendData();
void waitForRelease();

/* ========= BLE Callbacks ========= */
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE Client Connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected");
  }
};

void setup() {

  Serial.begin(115200);

  pinMode(irPin1, INPUT_PULLUP);
  pinMode(irPin2, INPUT_PULLUP);

  /* ========= Initialize BLE ========= */
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  /* Start advertising */
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  BLEDevice::startAdvertising();

  Serial.println("BLE Ready - Waiting for connection...");
}

void loop() {

  // ===== Detect Entry (Sensor A triggers first, then B) =====
  if (digitalRead(irPin1) == LOW) {

    unsigned long startTime = millis();

    while ((millis() - startTime) < timeout) {
      if (digitalRead(irPin2) == LOW) {
        Serial.println(">> Entry Detected");
        in_count++;
        updateCount();
        sendData();
        break;
      }
    }

    waitForRelease();
    delay(debounceDelay);
  }

  // ===== Detect Exit (Sensor B triggers first, then A) =====
  else if (digitalRead(irPin2) == LOW) {

    unsigned long startTime = millis();

    while ((millis() - startTime) < timeout) {
      if (digitalRead(irPin1) == LOW) {
        if (out_count < in_count) {
          Serial.println("<< Exit Detected");
          out_count++;
          updateCount();
          sendData();
          break;
        }
      }
    }

    waitForRelease();
    delay(debounceDelay);
  }
}

/* ========= Update Current Count ========= */
void updateCount() {
  current_count = in_count - out_count;

  Serial.print("IN: ");
  Serial.print(in_count);
  Serial.print(" OUT: ");
  Serial.print(out_count);
  Serial.print(" CURRENT: ");
  Serial.println(current_count);
}

/* ========= Send Data via BLE Notify ========= */
void sendData() {

  if (deviceConnected) {
    String data = String(current_count);
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
    Serial.println("BLE Data Sent: " + data);
  } else {
    Serial.println("No BLE client connected");
  }

  /* Handle reconnection: restart advertising when client disconnects */
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Restarted BLE advertising");
    oldDeviceConnected = false;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = true;
  }
}

/* ========= Wait for Both Sensors to Release (with timeout) ========= */
void waitForRelease() {
  unsigned long start = millis();
  while ((!digitalRead(irPin1) || !digitalRead(irPin2)) &&
         (millis() - start) < releaseTimeout) {
    delay(1);  // yield to prevent WDT reset
  }
}