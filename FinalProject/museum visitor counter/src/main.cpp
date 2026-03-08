#include <Arduino.h>
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
const int irPin1 = D1;
const int irPin2 = D2;

int in_count = 0;
int out_count = 0;
int current_count = 0;

const unsigned long timeout = 1000;
const unsigned long debounceDelay = 500;

void updateCount();
void sendData();

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
  delay(1000);
  Serial.println("Museum Visitor Counter");

  pinMode(irPin1, INPUT_PULLUP);
  pinMode(irPin2, INPUT_PULLUP);

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

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  BLEDevice::startAdvertising();

  Serial.println("BLE Ready - Waiting for connection...");
}

void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Restarted BLE advertising");
    oldDeviceConnected = false;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = true;
  }

  if (digitalRead(irPin1) == LOW) {
    Serial.println(">> Sensor A triggered - ENTRY");
    in_count++;
    updateCount();
    sendData();
    delay(debounceDelay);
    while (digitalRead(irPin1) == LOW) delay(10);
    delay(200);
  }

  if (digitalRead(irPin2) == LOW) {
    Serial.println("<< Sensor B triggered - EXIT");
    if (out_count < in_count) {
      out_count++;
      updateCount();
      sendData();
    }
    delay(debounceDelay);
    while (digitalRead(irPin2) == LOW) delay(10);
    delay(200);
  }
}

void updateCount() {
  current_count = in_count - out_count;
  Serial.print("IN: ");
  Serial.print(in_count);
  Serial.print(" OUT: ");
  Serial.print(out_count);
  Serial.print(" CURRENT: ");
  Serial.println(current_count);
}

void sendData() {
  if (deviceConnected) {
    String data = String(current_count);
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
    Serial.println("BLE Data Sent: " + data);
  } else {
    Serial.println("No BLE client connected");
  }
}
