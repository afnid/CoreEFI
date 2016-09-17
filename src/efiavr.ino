#include "System.h"
#include "Stream.h"
#include "Tasks.h"
#include "Hardware.h"

void setup() {
	Serial.begin(115200);
	Serial.println("CoreEFI v007a");
	Serial.println(getBuildVersion());

	initSystem(false);
}

void loop() {
	while (true)
		taskmgr.check();
}
