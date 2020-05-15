#include "DavisRFM69.h"
#include <SPI.h>

#define EXTERNAL_WATCHDOG_PIN PB11

#ifndef EXTERNAL_WATCHDOG_PIN
#include <libmaple/nvic.h>
#include <libmaple/iwdg.h>
#endif


// Select your modem:
#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

#define GSM_MODEM_POWER_PIN PA1
#define GSM_MODEM_SERIAL Serial2

// Telekom
/*
const char gsm_apn[]  = "internet.t-mobile";
const char gsm_user[] = "t-mobile";
const char gsm_pass[] = "tm";
*/

// O2
const char gsm_apn[]  = "internet";
const char gsm_user[] = "";
const char gsm_pass[] = "";

// Send data to 1-2 remote hosts
#include "remote_hosts.h"

#define SERIAL_BAUD   115200
#define PACKET_INTERVAL 2555

#define BUILTIN_LED PC13
#define LED_ON LOW
#define LED_OFF HIGH

// Optionales "define" falls ein externer 433 MHz Sensor, z.B. als Wassertemperatursensor verbaut werden soll
// (siehe weitere Projektdokumentation)
#define XTEMP_RECEIVER_PIN PB8

float rain_per_tip=0.2; // mm - je nach Geräte-Typ: 0.0254 oder 0.2

DavisRFM69 radio;

#ifdef XTEMP_RECEIVER_PIN
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile
RH_ASK xtemp_receiver(1000,XTEMP_RECEIVER_PIN,255,255,false);
#endif

TinyGsm modem(GSM_MODEM_SERIAL);
#ifdef REMOTE_USE_HTTPS
TinyGsmClientSecure client(modem);
#else
TinyGsmClient client(modem);
#endif

void watchdog_feed() {
  #ifdef EXTERNAL_WATCHDOG_PIN
    digitalWrite(EXTERNAL_WATCHDOG_PIN,LOW);
    delay(50);
    digitalWrite(EXTERNAL_WATCHDOG_PIN,HIGH);
  #else
    iwdg_feed();
  #endif
}

void setup() {
  #ifdef EXTERNAL_WATCHDOG_PIN
    pinMode(EXTERNAL_WATCHDOG_PIN,OUTPUT);
    digitalWrite(EXTERNAL_WATCHDOG_PIN,HIGH);
  #else
    iwdg_init(IWDG_PRE_256, 4095); // ~26 sekunden watchdog (maximum auf dem board)
  #endif
  pinMode(BUILTIN_LED,OUTPUT);
  blink(3,500,500);
  Serial.begin(SERIAL_BAUD);
  watchdog_feed(); // reset watchdog
  Serial.println("Starting");

  pinMode(GSM_MODEM_POWER_PIN,OUTPUT);
  modem_poweroff();
  watchdog_feed(); // reset watchdog

  // Empfänger vorbereiten. Da die Frequenz variabel ist, wird sie später im Sketch gesetzt
  radio.initialize();
  radio.setChannel(0);
  watchdog_feed(); // reset watchdog

  #ifdef XTEMP_RECEIVER_PIN
  xtemp_receiver.init();
  #endif
  watchdog_feed(); // reset watchdog
}

void blink(int count, int on, int off) {
  for (int i=0;i<count;i++) {
    digitalWrite(BUILTIN_LED,LED_ON);
    delay(on);
    digitalWrite(BUILTIN_LED,LED_OFF);
    delay(off);
  }
}
unsigned long led_off_ts;
void led_on(int time_ms) {
  digitalWrite(BUILTIN_LED,LED_ON);
  led_off_ts=millis()+time_ms;
  
}

void modem_poweroff() {
    blink(3,50,150);
    digitalWrite(GSM_MODEM_POWER_PIN,HIGH);
    delay(500);
    blink(3,50,150);
    digitalWrite(GSM_MODEM_POWER_PIN,LOW);
    GSM_MODEM_SERIAL.begin(115200);
    blink(3,50,150);
    delay(2500);
    watchdog_feed(); // reset watchdog
    Serial.println("Modem restart");
    blink(3,50,150);
    modem.restart();
    Serial.println("Modem init");
    blink(3,50,150);
    watchdog_feed(); // reset watchdog
    modem.init();
    Serial.println("Modem poweroff");
    blink(3,50,150);
    modem.poweroff();
    Serial.println("Modem offline.");
}

int stats_windCount=0;
float stats_windKmhMin=0;
float stats_windKmhMax=0;
float stats_windKmhMaxDirection=0;
double stats_windKmhSum=0;
int stats_windDirections[72];
float stats_temp=-100;
float stats_hum=-100;
float stats_windGustKmh=-100;
int stats_rainCount=0;
double stats_rainSum=0;
float stats_vCap=-100;
float stats_vSolar=-100;
// external temperature
float stats_xTemp=-100;
// external battery
float stats_xBat=-100;

void reset_stats() {
  stats_windCount=0;
  stats_windKmhMin=0;
  stats_windKmhMax=0;
  stats_windKmhMaxDirection=0;
  stats_windKmhSum=0;
  for (int i=0;i<72;i++) {
    stats_windDirections[i]=0;
  }
  stats_temp=-100;
  stats_hum=-100;
  stats_windGustKmh=-100;
  stats_rainCount=0;
  stats_rainSum=0;
  stats_vCap=-100;
  stats_vSolar=-100;
  stats_xTemp=-100;
  stats_xBat=-100;
}

#ifdef XTEMP_RECEIVER_PIN
// https://forum.arduino.cc/index.php?topic=189259.0
union xtemp_value {
       float f;
       struct {
           byte b1;
           byte b2;
           byte b3;
           byte b4;
       } b;
   };
union xtemp_value xtemp;
uint8_t xtemp_buf[12];
uint8_t xtemp_buflen = sizeof(xtemp_buf);

/**
 * Empfängt ein externes Temperatur+Batterie Datenpaket
 */
void xtemp_receive() {
  xtemp_buflen = sizeof(xtemp_buf);
  if (xtemp_receiver.recv(xtemp_buf, &xtemp_buflen)) { // Non-blocking
      if (xtemp_buflen==9 && xtemp_buf[0]==101) {
        xtemp.b.b1=xtemp_buf[4];
        xtemp.b.b2=xtemp_buf[5];
        xtemp.b.b3=xtemp_buf[6];
        xtemp.b.b4=xtemp_buf[7];
        int bat_byte=xtemp_buf[8];
        stats_xBat=(bat_byte-1)/100.0+2.9;
        stats_xTemp=xtemp.f;

        Serial.print(millis());
        Serial.print(F(" - Receive external temp: "));
        Serial.print(stats_xTemp);
        Serial.print(F(", battery: "));
        Serial.print(stats_xBat);
        Serial.println(F(" V"));
        led_on(1000);
      }
  }
}
#endif

/**
 * Dekodiert ein Datenpaket vom Radio.
 * Quellen des Paketformates:
 * https://github.com/dekay/DavisRFM69/wiki/Message-Protocol
 * https://github.com/bemasher/rtldavis/blob/master/protocol/protocol.go
 * https://github.com/matthewwall/weewx-meteostick/blob/master/bin/user/meteostick.py#L750
 */
void radio_decode() {
  led_on(300);
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

  int windDirectionIndex=int((windDirection+2.5)/5)%72;

  // Windrichtung wird im Array in einen 5° Raster einsortiert - die häufigste Windrichtung der letzten 5 Minuten wird dann verwendet
  stats_windDirections[windDirectionIndex]++;
  stats_windKmhSum+=windKmh;
  if (stats_windCount==0 || windKmh<stats_windKmhMin) {
    stats_windKmhMin=windKmh;
  }
  if (stats_windCount==0 || windKmh>stats_windKmhMax) {
    stats_windKmhMax=windKmh;
    stats_windKmhMaxDirection=windDirection;
  }
  stats_windCount++;
  
  Serial.print(millis());
  Serial.print(F(" - Station "));
  Serial.print(senderId);
  Serial.print(F(": Wind "));
  Serial.print(windKmh);
  Serial.print(F(" km/h aus Richtung "));
  Serial.print(windDirection);
  Serial.print(F("° (~"));
  Serial.print(windDirectionIndex*5);
  Serial.print(F("°)"));

  byte Byte3=radio.DATA[3];
  byte Byte4=radio.DATA[4];

  if (pkgType==2) {
    float supercap_v=((Byte3 * 4) + ((Byte4 && 0xC0) / 64)) / 300.0f;
    Serial.print(F("  SuperCap : "));
    Serial.print(supercap_v);
    Serial.println(F(" V"));
    stats_vCap=supercap_v;
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
    stats_rainCount++;
    stats_rainSum+=rain_rate;
  } else  if (pkgType==6) {
      Serial.print(F("  SolarRadiation:  "));
      Serial.println((((radio.DATA[3] << 8) + radio.DATA[4]) >> 6) * 1.757936);
    } else
    if (pkgType==8) {
      Serial.print(F("  Temperature (°C):  "));
      float tempF=(radio.DATA[3] * 256 + radio.DATA[4]) / 160.0;
      float tempC=(tempF-32)*5.0/9.0;
      Serial.println(tempC);
      stats_temp=tempC;
    } else
    if (pkgType==7) {
      int solar_power_raw = ((radio.DATA[3] << 2) + (radio.DATA[4] >> 6)) & 0x3FF;
      if (solar_power_raw != 0x3ff) {
        float solar_power_voltage=solar_power_raw / 300.0f;
        Serial.print(F("  Solar power (V):  "));
        Serial.println(solar_power_voltage);
        stats_vSolar=solar_power_voltage;
      }
    } else
    if (pkgType==9) {
      Serial.print(F("  Wind Gust km/h:  "));
      float windGustKmh=radio.DATA[3]*1.609344;
      Serial.println(windGustKmh);
      if (windGustKmh>stats_windGustKmh) {
        stats_windGustKmh=windGustKmh;
      }
    }
    if (pkgType==0xA) {
      float humidity = (((radio.DATA[4] >> 4) << 8) + radio.DATA[3]) / 10.0;
      Serial.print(F("  Rel humidity %:  "));
      Serial.println(humidity);
      stats_hum=humidity;
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

// das 1. Mal wird nach 2-3 Minuten gesendet. Dadurch wird bei einem Watchdog-Reset das Intervall nicht zu groß
unsigned long lastSendTime = millis()+120000;
char send_buffer[255];

void send_data() {
  if ((millis() - lastSendTime) > 300000 && (stats_windCount>0||stats_xTemp>-100)) {
    Serial.println(F("Sending data"));

    int pos=0;

    if (stats_windCount>0) {
      Serial.print(F("  Wind min km/h: "));
      Serial.println(stats_windKmhMin);
      Serial.print(F("  Wind max km/h: "));
      Serial.println(stats_windKmhMax);
      Serial.print(F("  Wind max direction: "));
      Serial.println(stats_windKmhMaxDirection);
      Serial.print(F("  Wind avg km/h: "));
      Serial.println(stats_windKmhSum/stats_windCount);

      // Häufigste Windrichtung der letzten 5 Minuten ermitteln
      int windDirectionIndex=-1;
      int windDirectionMaxCount=0;
      for (int i=0;i<72;i++) {
        byte count=stats_windDirections[i];
        if (count>windDirectionMaxCount) {
          windDirectionMaxCount=count;
          windDirectionIndex=i;
        }
      }

      pos+=sprintf(send_buffer+pos,"&up=%lu&wmi=%0.1f&wma=%0.1f&wmd=%0.1f&wav=%0.1f",millis(),stats_windKmhMin,stats_windKmhMax,stats_windKmhMaxDirection,
        stats_windKmhSum/stats_windCount);

      if (windDirectionIndex>-1) {
        Serial.print(F("  Wind main direction: "));
        Serial.println(windDirectionIndex*5);
        pos+=sprintf(send_buffer+pos,"&wd=%i",windDirectionIndex*5);
      }
        
    }
    
    if (stats_windGustKmh>-100) {
      pos+=sprintf(send_buffer+pos,"&wg=%0.1f",stats_windGustKmh);
    }
    if (stats_temp>-100) {
      pos+=sprintf(send_buffer+pos,"&t=%0.1f",stats_temp);
    }
    if (stats_hum>-100) {
      pos+=sprintf(send_buffer+pos,"&h=%0.1f",stats_hum);
    }
    if (stats_rainCount>0) {
      pos+=sprintf(send_buffer+pos,"&r=%0.1f",stats_rainSum/stats_rainCount);
    }
    if (stats_vCap>-100) {
      pos+=sprintf(send_buffer+pos,"&vc=%0.1f",stats_vCap);
    }
    if (stats_vSolar>-100) {
      pos+=sprintf(send_buffer+pos,"&vs=%0.1f",stats_vSolar);
    }
    if (stats_xTemp>-100) {
      pos+=sprintf(send_buffer+pos,"&xt=%0.1f",stats_xTemp);
    }
    if (stats_xBat>-100) {
      pos+=sprintf(send_buffer+pos,"&xv=%0.1f",stats_xBat);
    }

    reset_stats();

    watchdog_feed(); // reset watchdog
    Serial.println(F("  Reset Modem..."));
    digitalWrite(GSM_MODEM_POWER_PIN,HIGH); // Power off modem
    delay(1000);
    digitalWrite(GSM_MODEM_POWER_PIN,LOW); // Power on modem
    watchdog_feed(); // reset watchdog

    gsm_send();

    watchdog_feed(); // reset watchdog
    modem_poweroff();
    
  }
}

char http_buffer[2048];
void gsm_send() {
  lastSendTime=millis();
  
  // Set GSM module baud rate
  GSM_MODEM_SERIAL.begin(115200);
  delay(3000);
    watchdog_feed(); // reset watchdog

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println(F("  Initializing modem..."));
  modem.restart();

  blink(3,50,150);
    watchdog_feed(); // reset watchdog
  if (!modem.init()) {
    Serial.println(F("  Cannot initialize modem!"));
    return;
  }
    watchdog_feed(); // reset watchdog

#ifdef REMOTE_USE_HTTPS
  if (!modem.hasSSL()) {
    Serial.print(F("  Modem has no SSL Support!"));
    return;
  }
#endif

  Serial.print(F("SIM Status: "));
  int simStatus=modem.getSimStatus();
  Serial.print(simStatus);
  Serial.print(" = ");
  switch (simStatus) {
    case 0:
      Serial.println("ERROR");
      return;
      break;
    case 1:
      Serial.println("READY");
      break;
    case 2:
      Serial.println("LOCKED");
      return;
      break;
    default:
      Serial.println("UNKNOWN");
      return;
  }
  watchdog_feed(); // reset watchdog

  blink(3,50,150);
  Serial.print(F("  Waiting for network "));
  int regStatus=modem.getRegistrationStatus();
  bool hasNetwork=false;
  for (int i=0;i<6;i++) {
    watchdog_feed(); // reset watchdog
    if (modem.waitForNetwork(10000)) {
      hasNetwork=true;
      break;
    } else {
      Serial.println(F("  No network"));
    }
  }    
  if (!hasNetwork) {
    return;
  }
  watchdog_feed(); // reset watchdog
  Serial.print(" OK, quality:");
  Serial.println(modem.getSignalQuality());

  Serial.print(F("  Connecting to APN: "));
  Serial.print(gsm_apn);
  if (!modem.gprsConnect(gsm_apn, gsm_user, gsm_pass)) {
    Serial.println(F(" failed"));
    return;
  }
  Serial.println(F(" OK"));
  watchdog_feed(); // reset watchdog

  http_send(REMOTE_SERVER1, REMOTE_PORT1,REMOTE_URI1);
  #ifdef REMOTE_SERVER2
  http_send(REMOTE_SERVER2,REMOTE_PORT2,REMOTE_URI2);
  #endif
  
  Serial.println(F("  DONE."));
  blink(5,150,150);
}

void http_send(char* server,int port,char* uri) {
  blink(3,50,150);
  Serial.print(F("  Connecting to server: "));
  Serial.print(server);
  if (!client.connect(server,port)) {
    Serial.println(F(" failed"));
    return;
  }
  Serial.println(F(" OK"));
  watchdog_feed(); // reset watchdog

  int pos=0;
  pos+=sprintf(http_buffer+pos,"GET ");
  pos+=sprintf(http_buffer+pos,uri);
  pos+=sprintf(http_buffer+pos,send_buffer);
  pos+=sprintf(http_buffer+pos," HTTP/1.0\r\n");
  pos+=sprintf(http_buffer+pos,"Host: ");
  pos+=sprintf(http_buffer+pos,server);
  pos+=sprintf(http_buffer+pos,"\r\nConnection: close\r\n\r\n");

  Serial.println(F("  Request: "));
  Serial.println(http_buffer);

  client.write((uint8_t*)http_buffer,pos);
  watchdog_feed(); // reset watchdog

  Serial.println(F("  Reading response"));

  blink(3,50,150);
  watchdog_feed(); // reset watchdog
  // Await the response, max 3 s
  delay(3000);
  /*
  unsigned long timeout = millis()+3000L;
  while (client.connected() && (timeout>millis())) {
    // Print available data
    while (client.available() && (timeout>millis())) {
      client.read();
    }
  }
  */
  watchdog_feed(); // reset watchdog
  client.stop();

  Serial.println(F("  Done."));
}

void loop() {
  watchdog_feed(); // reset watchdog
  if (millis()>led_off_ts) {
    digitalWrite(BUILTIN_LED,LED_OFF);
  }
  radio_receive();
  if ((millis() - lastRxTime)/1000>900) {
    Serial.println(F("No signal since 15 minutes - resetting myself"));
    delay(500);
    nvic_sys_reset();
    delay(500);
  }
  if (millis()>3*3600000) {
    Serial.println(F("Auto-Reset after 3 hours."));
    delay(500);
    nvic_sys_reset();
    delay(500);
  }
  #ifdef XTEMP_RECEIVER_PIN
  xtemp_receive();
  #endif
  send_data();
}

