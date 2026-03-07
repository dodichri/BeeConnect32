#include <Arduino.h>
#include "version.h"
#include "display.h"

void setup()
{
    Serial.begin(115200);
    Serial.printf("BeeConnect32 v%s booting...\n", FIRMWARE_VERSION);

    display_init();
    display_show_splash(FIRMWARE_VERSION);

    // Placeholder readings until sensors module is implemented (Phase 3)
    display_show_main(12.34f, 21.5f, 80, 100);
}

void loop()
{
    display_tick();
    delay(5);
}
