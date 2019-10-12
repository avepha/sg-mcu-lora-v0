//#define TRANSMITTER_TEST

#include <Arduino.h>
#include <RH_RF95.h>
#include <SoftwareSerial.h>

#include "config.h"

SoftwareSerial to485(TO485_RX, TO485_TX);
RH_RF95 rf95(RFM95_CS, RFM95_INT);
LORA_MODE_ENUM loraModeEnum = LORA_MODE_RECEIVER;

void blinkLedBlue(int delayTime) {
  digitalWrite(BLUE_LED, HIGH);
  delay(delayTime);
  digitalWrite(BLUE_LED, LOW);
  delay(delayTime);
}

void setLoRaMode(LORA_MODE_ENUM mode) {
  switch (mode) {
    case LORA_MODE_TRANSMITTER: {
      if (loraModeEnum != LORA_MODE_TRANSMITTER) {
        loraModeEnum = LORA_MODE_TRANSMITTER;
        digitalWrite(RS485_TRANS, HIGH); // set rs485 as transmitter
        delay(10);
      }
      break;
    }
    case LORA_MODE_RECEIVER: {
      if (loraModeEnum != LORA_MODE_RECEIVER) {
        loraModeEnum = LORA_MODE_RECEIVER;
        digitalWrite(RS485_TRANS, LOW); // set rs485 as receiver
        delay(10);
      }
      break;
    }
  }

}

void setup() {
  Serial.begin(9600);
  to485.begin(9600);
  pinMode(PWD_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  pinMode(RS485_TRANS, OUTPUT);

  // reset rf module
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    for (int i = 0; i < 8; i++) {
      blinkLedBlue(100);
    }
    delay(1000);
  }

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    for (int i = 0; i < 8; i++) {
      blinkLedBlue(100);
    }
    delay(1000);
  }

  rf95.setTxPower(23, false);
  digitalWrite(PWD_LED, HIGH); // solid pwd led
  setLoRaMode(LORA_MODE_RECEIVER);

  Serial.println("RFM95 Initialized!");
}

void loop() {
#ifdef TRANSMITTER_TEST
  setLoRaMode(LORA_MODE_TRANSMITTER);
  digitalWrite(BLUE_LED, HIGH);
  Serial.println("Sending to rf95_server");
  uint8_t data[] = "SG-Hello";
  rf95.send(data, sizeof(data));
  rf95.waitPacketSent();
  digitalWrite(BLUE_LED, LOW);
  delay(1000);
#else
  if (rf95.available()) {
    setLoRaMode(LORA_MODE_RECEIVER);
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      digitalWrite(BLUE_LED, HIGH);
      Serial.print("Got: ");
      Serial.println((char *) buf);
      to485.println((char *) buf);
      digitalWrite(BLUE_LED, LOW);
    }
    else {
      blinkLedBlue(10);
    }
  }

  if (to485.available()) {
    byte raw[64];
    int readIndex = 0;
    while (to485.available()) {
      if (to485.read() == 0xEE) {
        raw[readIndex++] = 0xEE;
        int loop = 0;
        while (true) {
          if (to485.available()) {
            byte byteVal = to485.read();

            if (byteVal == 0xEF) {
              raw[readIndex++] = 0xEF;
              Serial.print("RS485 Packet: ");
              for (int i = 0; i < readIndex; i++) {
                Serial.print(raw[i], HEX); Serial.print(" ");
              }
              Serial.println();

              Serial.print("Sending Packet...");
              setLoRaMode(LORA_MODE_TRANSMITTER);
              rf95.send(raw, readIndex);
              for (int i = 0 ; i < 2; i++) blinkLedBlue(30);
              rf95.waitPacketSent();
              setLoRaMode(LORA_MODE_RECEIVER);
              Serial.println("Success!");
              return;
            }
            else {
              raw[readIndex++] = byteVal;
            }
            loop = 0; // reset counter when data is coming
          }
          else {
            // ensure this process can not hold resource more than 50ms
            loop++;
            delay(1);
            if (loop >= 50) return;
          }
        }
      }
    }
  }

  delay(100);
#endif
}