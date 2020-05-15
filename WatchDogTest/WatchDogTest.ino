#include <libmaple/iwdg.h>

void setup() {
  Serial.begin(115200);
  iwdg_init(IWDG_PRE_256, 4095); // init an 26 second wd timer
}

void loop() {
  for (int i=0;;i++) {
    Serial.println(i);
    delay(1000);
    iwdg_feed();
  }

}
