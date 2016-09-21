#include "System.h"
#include "Tasks.h"
#include "Hardware.h"

void setup() {
	Serial.begin(115200);
	Serial.print("CoreEFI ");
	Serial.println(getBuildVersion());
	
	hardware.recv().nl("ps\n");
	
	extern void initPins();
	initPins();
	
	initSystem(false);
}

void loop() {
	Serial.println("loop");
	
	while (true)
		taskmgr.check();
}
