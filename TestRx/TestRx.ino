
#include "Manchester.h"

/*

  Manchester Receiver example
  
  In this example receiver will receive array of 10 bytes per transmittion

  try different speeds using this constants, your maximum possible speed will 
  depend on various factors like transmitter type, distance, microcontroller speed, ...

  MAN_300 0
  MAN_600 1
  MAN_1200 2
  MAN_2400 3
  MAN_4800 4
  MAN_9600 5
  MAN_19200 6
  MAN_38400 7

*/

#define RX_PIN PB8

uint8_t moo = 1;
#define BUFFER_SIZE 22
uint8_t buffer[BUFFER_SIZE];

void setup() 
{
  Serial.begin(115200);
  man.setupReceive(RX_PIN, MAN_1200);
  man.beginReceiveArray(BUFFER_SIZE, buffer);
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
  if (man.receiveComplete()) 
  {
    Serial.println("Received");
    Serial.println(buffer[0]);
    Serial.println(buffer[1]);
    Serial.println(buffer[2]);
    Serial.println(buffer[3]);
    Serial.println(buffer[4]);
    Serial.println(buffer[5]);
    Serial.println(buffer[6]);
    Serial.println(buffer[7]);
    Serial.println("--");
    temp.b.b1=buffer[4];
    temp.b.b2=buffer[5];
    temp.b.b3=buffer[6];
    temp.b.b4=buffer[7];
    Serial.println(temp.f);

    man.beginReceiveArray(BUFFER_SIZE, buffer);
    moo = ++moo % 2;
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
