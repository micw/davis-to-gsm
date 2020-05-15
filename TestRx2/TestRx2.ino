#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

#define RX_PIN PB8

RH_ASK receiver(1000,RX_PIN,255,255,false);

void setup() 
{
  Serial.begin(115200);
  delay(2000);
  Serial.println(receiver.init());
  Serial.println("Ready.");
}

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

void loop() 
{
    uint8_t buf[12];
    uint8_t buflen = sizeof(buf);
    if (receiver.recv(buf, &buflen)) // Non-blocking
    {
      Serial.print("Rec ");
      Serial.print(buflen);
      Serial.println(" bytes");
      if (buflen==9 && buf[0]==101) {
        temp.b.b1=buf[4];
        temp.b.b2=buf[5];
        temp.b.b3=buf[6];
        temp.b.b4=buf[7];
        unsigned long t=millis()/1000;
        unsigned long h=t/3600;
        unsigned long m=(t/60)%60;
        unsigned long s=t%60;

        int bat_byte=buf[8];
        float bat=(bat_byte-1)/100.0+2.9;


        if (h<100) Serial.print(" ");
        if (h<10) Serial.print(" ");
        Serial.print(h);
        Serial.print(":");
        if (m<10) Serial.print("0");
        Serial.print(m);
        Serial.print(":");
        if (s<10) Serial.print("0");
        Serial.print(s);
        Serial.print(" - temperature: ");
        Serial.print(temp.f);
        Serial.print(", battery: ");
        Serial.print(bat);
        Serial.println(" V");
        
      }
    }
}

/*
101
40
255
184
0
0
209
65
*/
