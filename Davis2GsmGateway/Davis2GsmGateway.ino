#include "DavisRFM69.h"
#include <SPI.h>

#define SERIAL_BAUD   115200
#define PACKET_INTERVAL 2555

float rain_per_tip=0.0254; // mm - je nach Geräte-Typ: 0.0254 oder 0.2

DavisRFM69 radio;

void setup() {
  delay(3000);
  Serial.begin(SERIAL_BAUD);
  Serial.println("Starting");

  // Empfänger vorbereiten. Da die Frequenz variabel ist, wird sie später im Sketch gesetzt
  radio.initialize();
  radio.setChannel(0);
}

/**
 * Dekodiert ein Datenpaket vom Radio.
 * Quellen des Paketformates:
 * https://github.com/dekay/DavisRFM69/wiki/Message-Protocol
 * https://github.com/bemasher/rtldavis/blob/master/protocol/protocol.go
 * https://github.com/matthewwall/weewx-meteostick/blob/master/bin/user/meteostick.py#L750
 */
void radio_decode() {
  Serial.print(millis());
  Serial.print(F(" - Decoding: "));
  for (byte i = 0; i < DAVIS_PACKET_LEN; i++) {
    Serial.print(radio.DATA[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();

  // Das erste Byte enthält die Station-ID (relevant bei mehr als einer Davis) sowie den Paket-Type
  int senderId = radio.DATA[0] & 0x0f;
  int pkgType= (radio.DATA[0] >> 4) & 0x0f;

  // Windgeschwindigkeit und Richtung werden mit jedem Paket geschickt
  float windMph=radio.DATA[1];
  float windKmh=windMph*1.609344;
  float windDirection;
  if (radio.DATA[2] == 0) {
    windDirection = 360;
  } else {
    windDirection = (radio.DATA[2] * 1.40625) + .3;
  }
  Serial.print(millis());
  Serial.print(F(" - Station "));
  Serial.print(senderId);
  Serial.print(F(": Wind "));
  Serial.print(windMph);
  Serial.print(" Meilen/h aus Richtung ");
  Serial.print(windDirection);

  byte Byte3=radio.DATA[3];
  byte Byte4=radio.DATA[4];

  if (pkgType==2) {
    float supercap_v=((Byte3 * 4) + ((Byte4 && 0xC0) / 64)) / 100.0f;
    Serial.print(F("  SuperCap : "));
    Serial.print(supercap_v);
    Serial.println(F(" V"));
  } else if (pkgType==4) {
    float uv_index=(((Byte3 << 8) + Byte4) >> 6) / 50.0f;
    Serial.print(F("  UV-Index : "));
    Serial.println(uv_index);
  } else if (pkgType==5) {
    // https://github.com/matthewwall/weewx-meteostick/blob/master/bin/user/meteostick.py#L784
    int time_between_tips_raw = ((Byte4 & 0x30) << 4) + Byte3;
    float rain_rate = -1;
    if (time_between_tips_raw == 0x3FF) {
      // no rain
      rain_rate = 0;
    } else if (Byte4  & 0x40 == 0) {
      // heavy rain. typical value: 64/16 - 1020/16 = 4 - 63.8 (180.0 - 11.1 mm/h)
      rain_rate = 3600.0 / ( time_between_tips_raw / 16.0) * rain_per_tip;
    } else {
      // light rain. typical value 64 - 1022 (11.1 - 0.8 mm/h)
      rain_rate = 3600.0 / time_between_tips_raw * rain_per_tip;
    }
    Serial.print(F("  Regen : "));
    Serial.print(rain_rate);
    Serial.println(F(" mm/h"));
  } else  if (pkgType==6) {
      Serial.print(F("  SolarRadiation:  "));
      Serial.println((((radio.DATA[3] << 8) + radio.DATA[4]) >> 6) * 1.757936);
    } else
    if (pkgType==8) {
      Serial.print(F("  Temperature (F):  "));
      Serial.println((radio.DATA[3] * 256 + radio.DATA[4]) / 160);
    } else
    if (pkgType==9) {
      Serial.print(F("  Wind Gust:  "));
      Serial.println(radio.DATA[3]);
    }
    else {
      Serial.println();
    }

  
}

unsigned long lastRxTime = 0;
byte hopCount = 0;
boolean goodCrc = false;
int16_t goodRssi = -999;
/**
 * Wartet auf ein Signal von der Wetterstation.
 * Die Station sendet der Reihe nach auf verschiedenen Kanälen (4 in Deutschland). Der Empfänger muss dazu immer nachgezogen werden.
 * Der Code springt bei einem erfolgreich empfangenen Paket zum nächten Kanal. Wird nichts empfangen, wird gesucht, bis wieder ein Paket empfangen wurde.
 * Der Originalcode ist unter https://github.com/dekay/DavisRFM69/blob/esp/examples/ISSRx/ISSRx.ino zu finden.
 */
void radio_receive() {
  if (radio.receiveDone()) {

    Serial.print(millis());
    Serial.print(F(":  "));
    Serial.print(radio.CHANNEL);
    Serial.print(F("  RSSI: "));
    Serial.println(radio.RSSI);
  
    unsigned int crc = radio.crc16_ccitt(radio.DATA, 6);
    if ((crc == (word(radio.DATA[6], radio.DATA[7]))) && (crc != 0)) {
      // This is a good packet
      goodCrc = true;
      goodRssi = radio.RSSI;
      packetStats.receivedStreak++;
      hopCount = 1;
    } else {
      goodCrc = false;
      packetStats.crcErrors++;
      packetStats.receivedStreak = 0;
    }
    if (goodCrc && (radio.RSSI < (goodRssi + 15))) {
      radio_decode();
      lastRxTime = millis();
      radio.hop();
      Serial.print(millis());
      Serial.println(F(":  Hopped channel and ready to receive."));
    } else {
      radio.waitHere();
      Serial.print(millis());
      Serial.println(F(":  Waiting here"));
    }
  }
  // If a packet was not received at the expected time, hop the radio anyway
  // in an attempt to keep up.  Give up after 4 failed attempts.  Keep track
  // of packet stats as we go.  I consider a consecutive string of missed
  // packets to be a single resync.  Thx to Kobuki for this algorithm.
  if ((hopCount > 0) && ((millis() - lastRxTime) > (hopCount * PACKET_INTERVAL + 200))) {
    packetStats.packetsMissed++;
    if (hopCount == 1) packetStats.numResyncs++;
    if (++hopCount > 4) hopCount = 0;
    radio.hop();
    Serial.print(millis());
    Serial.println(F(":  Hopped channel and ready to receive."));
 }

}

void loop() {
  radio_receive();

}

