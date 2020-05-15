/**
 * Verwandelt einen preiswerten ESP8266 in einen Hardware-Watchdog
 * 
 * Verbindungen STM32 > ESP8266
 * - GND -> GND
 * - 5V -> 5V
 * - PB11 -> D6
 * - RST -> D7
 * 
 */

#include <ESP8266WiFi.h>

#define WATCHDOG_PIN D6
#define RESET_PIN D7

// warn after 60s
#define WARN_AFTER 60000
// reset after another 180s
#define RESET_AFTER 180000

int blink_pattern_off[]={0};
int blink_pattern_refresh[]={7,10,250,10,250,10,1000,-5};
int blink_pattern_warn[]={7,500,100,500,100,500,1000,-1};

int* current_blink_pattern=blink_pattern_off;
int current_blink_pattern_size=0;
int current_blink_pattern_pos=0;
unsigned long current_blink_pattern_pos_until=0;

void set_blink(int pattern[]) {
  current_blink_pattern=pattern;
  current_blink_pattern_pos=0;
  current_blink_pattern_size=pattern[0];
  current_blink_pattern_pos_until=0;
}

void process_blink() {
  if (millis()>current_blink_pattern_pos_until) {
    if (current_blink_pattern_pos>current_blink_pattern_size) {
      digitalWrite(BUILTIN_LED,HIGH);
      return;
    }
    current_blink_pattern_pos++;
    //Serial.println(current_blink_pattern_pos);
    if (current_blink_pattern_pos<=current_blink_pattern_size) {
      int next_interval=current_blink_pattern[current_blink_pattern_pos];
      if (next_interval<=0) {
        current_blink_pattern_pos=-next_interval;
        next_interval=current_blink_pattern[current_blink_pattern_pos];
      }
      current_blink_pattern_pos_until=millis()+next_interval;
      digitalWrite(BUILTIN_LED,(current_blink_pattern_pos+1)%2);
    }
  }
}

unsigned long next_state_switch;
unsigned long last_refresh;

int wdState;
void wd_refresh() {
  // blink if the last refest is >5s ago
  if (millis()-last_refresh>5000) {
    Serial.println("refresh");
    set_blink(blink_pattern_refresh);
    last_refresh=millis();
  }
  wdState=0; // OK
  next_state_switch=millis() + WARN_AFTER;
}

void wd_reset() {
  digitalWrite(BUILTIN_LED,LOW);
  delay(2000);
  digitalWrite(RESET_PIN,LOW);
  delay(500);
  digitalWrite(RESET_PIN,HIGH);
  digitalWrite(BUILTIN_LED,HIGH);
}

void wd_check() {
  if (next_state_switch<millis()) {
    if (wdState==0) {
          set_blink(blink_pattern_warn);
    } else {
      Serial.println("reset");
      wd_reset();
    }
    Serial.println("warn");
    wdState=1; // WARN
    next_state_switch=millis()+RESET_AFTER;
  }
}


void setup() {
  pinMode(WATCHDOG_PIN,INPUT_PULLUP);
  pinMode(RESET_PIN,OUTPUT);
  digitalWrite(RESET_PIN,HIGH);
  pinMode(BUILTIN_LED,OUTPUT);
  digitalWrite(BUILTIN_LED,HIGH);
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  wd_refresh();
}

int oldPinState=-1;
void loop() {
  boolean state=digitalRead(WATCHDOG_PIN);
  if (state!=oldPinState) {
    if (state==LOW) {
      wd_refresh();
    }
    oldPinState=state;
  }
  wd_check();
  process_blink();
}
