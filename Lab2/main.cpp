#include <Arduino.h>

const int pinVOUT1 = A0;
const int pinVOUT2 = A1;

void setup() {
  Serial.begin(115200);
  
  analogReadResolution(12);
}

void loop() {
  int raw1 = analogRead(pinVOUT1);
  int raw2 = analogRead(pinVOUT2);

  float v1 = (raw1 / 4095.0) * 3.3;
  float v2 = (raw2 / 4095.0) * 3.3;

  Serial.print("VOUT1: ");
  Serial.print(v1, 2);
  Serial.print("V  VOUT2: ");
  Serial.print(v2, 2);
  Serial.println("V");

  delay(500);
}