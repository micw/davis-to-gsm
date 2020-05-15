/**
 * Low-Power remote temperature sensor. 
 * https://www.letscontrolit.com/wiki/index.php/Tutorial_Battery_Powered_Devices
 * 
 * Zielstellung:
 * - Temperatursensor mit autonomen Betrieb (Batterie+Solarzelle)
 * 
 * Stückliste:
 * - Wemos D1 mini
 *   - hat einen low-drop voltage regulator (0.25V drop) -> Läuft an einer Li-Ion Battery von 4.2 bis 3.3V (https://www.richtek.com/assets/product_file/RT9013/DS9013-10.pdf)
 *   - verbraucht nur 0.4mA im Deep-Sleep Modus
 * - TP4056 Lademodul für Lithium-Akkus
 *   - Variante mit Schutzschaltung und Ausgang!
 *   - Ebay Deutschland ~5€ oder AliExpress <1€
 * - DS18S20 Temperatursensor
 * - 433 MHz Transmitter
 * - Li.Ion Akku (z.B. 18650) oder LiPo Akku mit ~1000mAh Kapazität (Akku reicht für einige Tage)
 * - Solarzelle 5V, ca. 5W (Lädt den Akku an einem sonnigen Tag komplett)
 * - 1 Jumper
 * - 2-3 kOhm Widerstand
 * - Lochrasterplatine
 * - Optional
 *   - LED + 150-300 Ohm Vorwiderstand
 *   - Schraubklemmen
 *   
 * Schaltung:
 * - Die Solarzelle wird am Eingang des Ladereglers angeschlossen
 * - Der Akku wird am Laderegler (B+, B-) angeschlossen
 * - Der WeMos wird an den Laderegler angeschlossen (Out+ -> VIn, Out- -> GND)
 * - Das 433 MHZ Modul wird an GND -> GND, VCC -> Laderegler Out+ und Data -> Wemos an D7 angeschlossen
 *   - Durch den Anschluss an den Lederegler-Ausgang statt an 3.3V erhöht sich die Sendeleistung und damit die Reichweite etwas
 *   - Am Antennenausgang sollte ein 173 mm langer Draht als Antenne angelötet werden
 * - Der DS18S20 wird (am besten über die Schraubklemmen) wie folgt angeschlossen
 *   - https://datasheets.maximintegrated.com/en/ds/DS18S20.pdf
 *   - Am Wemos zwischen 3.3V und D5 wird der 2-3 kOhm Widerstand als Pullup geschaltet. Laut Datenblatt ist ein 4,7 kOhm zu verwenden. Ich verwendet
 *     einen 2,2 kOhm, damit kann man etwas längere Kabel verwenden
 *   - 3-Ader-Variante:
 *     - DS1820 GND -> Wemos GND
 *     - DS1820 VCC -> Wemos 3.3V
 *     - DS1820 DATA -> Wemos D5
 *   - 2-Ader-Variante:
 *     - DS1820 GND wird mit DS1820 VCC verbunden (äußere beide Pins)
 *     - DS1820 GND -> Wemos GND
 *     - DS1820 DATA -> Wemos D5
 * - Reset-Pin
 *   - Damit der Wemos wieder aus dem Deep-Sleep erwachen kann, muss Pin D0 mit Reset verbunden werden.
 *   - Mit dieser Schaltung ist alelrdings kein Flashen mehr möglich
 *   - Daher wird beides mit einem Jumper gebrückt, den man zum Flashen entfernen kann
 * - Optionale Status-LED
 *   - Wemos GND -> 100-300 Ohm Vorwiderstand -> LED -> Wemos D6
 *   
 * Testmessung Entladen (30s Sendeintervall, 1700 mAh Li-Ion Akku 18650)
 * - Start voll aufgeladen (4,11V)
 * - Stand nach 5 Tagen: 3,96V (ca. >85% Restkapazität)
 * - reiner Batteriebetrieb ist ca. Wochen lang möglich, mit besseren Akkus auch 8 Wochen
 * 
 * Testmessung Laden
 * - Start: 3,96V (Akkustand anch 5 Tagen Batteriebetrieb) 10:00
 * - Im Schatten (Terrasse), Solarzelle liefert ca. 4,4V
 * - 
 * 
 **/

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

// https://escapequotes.net/esp8266-wemos-d1-mini-pins-and-diagram/
#define D5 14
#define D6 12
#define D7 13

#define LED D6
#define ONE_WIRE_BUS D5
#define TRANSMITTER_PIN D7

// https://arduinodiy.wordpress.com/2016/12/25/monitoring-lipo-battery-voltage-with-wemos-d1-minibattery-shield-and-thingspeak/
#define BAT A0
#define BAT_R_kOHM 220


#define DEBUG

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress thermometer;
bool thermometerFound;

RH_ASK transmitter(1000,255,TRANSMITTER_PIN,255,false);


void setup() {
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

#ifdef LED
  pinMode(LED, OUTPUT);
#endif

#ifdef BAT
  pinMode(BAT, INPUT);
#endif

  sensors.begin();

#ifdef DEBUG
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Starting");
  Serial.println();
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
#endif

  thermometerFound=oneWire.search(thermometer);

  if (thermometerFound) {
    /**
     * Dauern und Auflösung Temperaturmessung. Höhere Genauigkeit -> größere Dauer -> mehr Akkuverbrauch
     * 9  Bit: 0,5°C, 0.09s
     * 10 Bit: 0.25°C, 0,19s
     * 11 Bit: 0,125°C, 0,375s
     * 12 Bit: 0,0625°C,  0,75s
     */
    sensors.setResolution(thermometer, 11);
#ifdef DEBUG
    Serial.print("Temperature sensor found: ");
    for (uint8_t i = 0; i < 8; i++) {
      if (thermometer[i] < 16) Serial.print("0");
      Serial.print(thermometer[i], HEX);
    }
    Serial.println();
  } else {
    Serial.println("!!!! TEMPERATURE SENSOR NOT FOUND !!!!");
#endif
  }

  transmitter.init();
}

byte data[9];

// https://forum.arduino.cc/index.php?topic=189259.0
union temp_value {
       float f;
       struct {
           byte b1;
           byte b2;
           byte b3;
           byte b4;
       } b;
   };
union temp_value temp;

void loop() {
  if (thermometerFound) {
#ifdef LED
    digitalWrite(LED, HIGH);
#endif
#ifdef DEBUG
    Serial.print("Requesting temperatures...");
#endif
    sensors.requestTemperatures(); // Send the command to get temperatures
    temp.f=sensors.getTempCByIndex(0);
#ifdef DEBUG
    Serial.println("DONE");
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(temp.f);
#endif
    // 1. Byte 101=Temperaturwert
    data[0]=101;
    // Byte 2-4: 3 Byte SensorID
    data[1]=thermometer[0];
    data[2]=thermometer[1];
    data[3]=thermometer[2];
    data[4]=temp.b.b1;
    data[5]=temp.b.b2;
    data[6]=temp.b.b3;
    data[7]=temp.b.b4;

    float vcc=analogRead(BAT)/1024.0f*5.4f;
#ifdef DEBUG
    Serial.print("VCC: ");
    Serial.print(vcc);
    Serial.println(" V");
#endif
    byte vcc_byte;
    if (vcc<2.9 || vcc>5.4) {
      vcc_byte=0; // error
    } else {
      // um 2,9V verschieben um den Wertebereich eines Bytes auszunutzen. 1 addieren, damit 0 für Fehler benutzt werden kann. 0.5 addieren, um korrekt zu runden
      vcc_byte=(unsigned byte) ((vcc-2.9f)*100.0f+1.5);
    }
    // umkehr: (vcc_byte-1)/100.0+2.9
    data[8]=vcc_byte;
    
    transmitter.send((uint8_t *)data, 9);
    transmitter.waitPacketSent();
#ifdef LED
    digitalWrite(LED, LOW);
  } else {
    for (int i=0;i<5;i++) {
      digitalWrite(LED, HIGH);
      delay(50);
      digitalWrite(LED, LOW);
      delay(150);
    }
#endif
  }
  
#ifdef DEBUG
  Serial.println("Data sent. Going to sleep.");
  // im Debugmodus warten bis serielle Daten weggeschrieben wurden!
  Serial.flush();
#endif
  ESP.deepSleep(30e6);
  //delay(5);
}
